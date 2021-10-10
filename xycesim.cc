/*************************************************************************
 *
 *  Copyright (c) 2021 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#include "xycesim.h"
#include "config_pkg.h"
#include <common/config.h>

#ifdef FOUND_N_CIR_XyceCInterface

#include <N_CIR_XyceCInterface.h>

#endif

XyceActInterface *XyceActInterface::_single_inst = NULL;


XyceActInterface::XyceActInterface()
{
  if (_single_inst) {
    fatal_error ("Creating multiple copies of a XyceActInterface?");
  }

  _xyce_ptr = NULL;
  _xycetime = 0.0;
  A_INIT (_wave);
  _to_xyce = NULL;
  _from_xyce = NULL;
  A_INIT (_analog_inst);
  _single_inst = this;

  config_set_default_real ("sim.timescale", 1e-12);
  config_set_default_real ("sim.analog_window", 0.05);
  config_set_default_real ("sim.settling_time", 1e-12);

  char *fname = config_file_name ("lint.conf");
  if (fname) {
    config_read ("lint.conf");
    FREE (fname);
  }

  _Vdd = config_get_real ("lint.Vdd");

  /* units: V/ns */
  _slewrate = 1/(0.5*1.0/config_get_real ("lint.slewrate_fast_threshold") +
		 0.5*1.0/config_get_real ("lint.slewrate_slow_threshold") );

  /* simulation unit conversion between integers and analog sim */
  _timescale = config_get_real ("sim.timescale");
  _percent = config_get_real ("sim.analog_window");
  if (_percent < 0 || _percent > 1) {
    fatal_error ("sim.analog_window parameter must be in [0,1]\n");
  }
  _settling_time = config_get_real ("sim.settling_time");
}

void XyceActInterface::_addProcess (XyceSim *xc)
{
  A_NEW (_analog_inst, XyceSim *);
  A_NEXT (_analog_inst) = xc;
  A_INC (_analog_inst);
}

void XyceActInterface::initXyce ()
{
  if (A_LEN (_analog_inst) == 0) {
    return;
  }
	     
#ifndef FOUND_N_CIR_XyceCInterface
  fatal_error ("Xyce interface not found at compile time.");
#else
  xyce_open (&_xyce_ptr);
  

  /* -- create spice netlist -- */

  Act *a = actsim_Act();
  ActPass *ap;
  ActNetlistPass *nl;
  netlist_t *top_nl;
  char buf[10240];

  ap = a->pass_find ("prs2net");
  if (ap) {
    nl = dynamic_cast<ActNetlistPass *> (ap);
    Assert (nl, "Hmm");
  }
  else {
    nl = new ActNetlistPass (a);
  }
  nl->run (actsim_top());

  nl->mkStickyVisited ();

  FILE *sfp = fopen ("_xyce.sp", "w");
  if (!sfp) {
    fatal_error ("Could not open file `_xyce.sp' for writing");
  }
  fprintf (sfp, "*\n* auto-generated by actsim\n*\n");

  top_nl = nl->getNL (actsim_top());

  int found_vdd = 0, found_gnd = 0;

  for (int i=0; i < A_LEN (top_nl->bN->used_globals); i++) {
    ActId *tid = top_nl->bN->used_globals[i]->toid();
    tid->sPrint (buf, 10240);
    delete tid;
    if (strcmp (buf, "Vdd") == 0) {
      found_vdd = 1;
    }
    else if (strcmp (buf, "GND") == 0) {
      found_gnd = 1;
    }
    fprintf (sfp, ".global ");
    a->mfprintf (sfp, "%s", buf);
    fprintf (sfp, "\n");
  }

  if (!found_vdd) {
    fprintf (sfp, ".global Vdd\n");
  }
  fprintf (sfp, "vvs0 Vdd dc %gV\n", _Vdd);

  if (!found_gnd) {
    fprintf (sfp, ".global GND\n");
  }
  fprintf (sfp, "vvs1 GND dc 0.0V\n\n");

  fprintf (sfp, "* --- include models ---\n\n");
  fprintf (sfp, ".inc \"%s\"\n", config_get_string ("sim.model_files"));

  fprintf (sfp, "*\n* -- printing any spice bodies needed --\n*\n");

  config_set_int ("net.emit_parasitics", 1);

  for (int i=0; i < A_LEN (_analog_inst); i++) {
    XyceSim *xs = _analog_inst[i];
    nl->Print (sfp, xs->getProc());
  }
  nl->clrStickyVisited ();

  fprintf (sfp, "*\n* instances\n*\n");
  
  for (int i=0; i < A_LEN (_analog_inst); i++) {
    XyceSim *xs = _analog_inst[i];
    ActId *inst = xs->getName();
    netlist_t *n = nl->getNL (xs->getProc());

    Assert (n, "What?");

    fprintf (sfp, "X%d ", i);
    for (int j=0; j < A_LEN (n->bN->ports); j++) {
      int len;
      if (n->bN->ports[j].omit) continue;

      inst->sPrint (buf, 10240);
      len = strlen (buf);
      snprintf (buf+len, 10240-len, ".");
      len += strlen (buf+len);
      
      ActId *tid;
      tid = n->bN->ports[j].c->toid();
      tid->sPrint (buf+len, 10240-len);
      delete tid;

      a->mfprintf (sfp, buf);
      fprintf (sfp, " ");

      int off = xs->getOffset (n->bN->ports[j].c);

      if (n->bN->ports[j].input) {
	/* DAC */
	ihash_bucket_t *b;
	struct xycefanout *xf;
	if (!_to_xyce) {
	  _to_xyce = ihash_new (4);
	}
	b = ihash_lookup (_to_xyce, off);
	if (b) {
	  xf = (struct xycefanout *) b->v;
	}
	else {
	  b = ihash_add (_to_xyce, off);
	  NEW (xf, struct xycefanout);
	  A_INIT (xf->dac_id);
	  b->v = xf;
	}
	A_NEW (xf->dac_id, char *);
	A_NEXT (xf->dac_id) = Strdup (buf);
	A_INC (xf->dac_id);
      }
      else {
	/* ADC */
	hash_bucket_t *b;
	
	if (!_from_xyce) {
	  _from_xyce = hash_new (4);
	}

	b = hash_lookup (_from_xyce, buf);
	if (b) {
	  warning ("Signal `%s' has a duplicate driver in Xyce!", buf);
	}
	else {
	  b = hash_add (_from_xyce, buf);
	  b->i = off;
	}
      }
    }
    a->mfprintfproc (sfp, xs->getProc());
    fprintf (sfp, "\n");
  }

  fprintf (sfp, "*\n* ADCs and DACs\n*\n");

  fprintf (sfp, ".model myADC ADC (settlingtime=%g uppervoltagelimit=%g lowervoltagelimit=%g)\n", _settling_time, _Vdd*(1-_percent), _Vdd*_percent);
  fprintf (sfp, ".model myDAC DAC (tr=%g tf=%g)\n",
	   (_Vdd/_slewrate)*1e-9, (_Vdd/_slewrate)*1e-9);
  
  /* -- now we need the ADCs and DACs -- */
  if (_to_xyce) {
    ihash_iter_t it;
    ihash_bucket_t *b;
    ihash_iter_init (_to_xyce, &it);
    while ((b = ihash_iter_next (_to_xyce, &it))) {
      struct xycefanout *xf;
      xf = (struct xycefanout *) b->v;
      for (int i=0; i < A_LEN (xf->dac_id); i++) {
	a->mfprintf (sfp, "YDAC %s %s 0 myDAC\n", xf->dac_id[i], xf->dac_id[i]);
      }
    }
  }

  if (_from_xyce) {
    hash_iter_t it;
    hash_bucket *b;
    hash_iter_init (_from_xyce, &it);
    while ((b = hash_iter_next (_from_xyce, &it))) {
      a->mfprintf (sfp, "YADC %s %s 0 myADC\n", b->key, b->key);
    }
  }

  fprintf (sfp, ".tran 0 1\n");

  fprintf (sfp, ".print tran format=raw\n");

  if (_to_xyce) {
    ihash_iter_t it;
    ihash_bucket_t *b;
    ihash_iter_init (_to_xyce, &it);
    while ((b = ihash_iter_next (_to_xyce, &it))) {
      struct xycefanout *xf;
      xf = (struct xycefanout *) b->v;
      for (int i=0; i < A_LEN (xf->dac_id); i++) {
	fprintf (sfp, "+ v(");
	a->mfprintf (sfp, "%s", xf->dac_id[i]);
	fprintf (sfp, ")\n");
      }
    }
  }
  if (_from_xyce) {
    hash_iter_t it;
    hash_bucket *b;
    hash_iter_init (_from_xyce, &it);
    while ((b = hash_iter_next (_from_xyce, &it))) {
      fprintf (sfp, "+ v(");
      a->mfprintf (sfp, "%s", b->key);
      fprintf (sfp, ")\n");
    }
  }
  fprintf (sfp, "\n");

  fprintf (sfp, ".end\n");
  fclose (sfp);

  /* -- initialize waveforms -- */
  const char *_cinit_xyce[] =
    { "Xyce", "-quiet", "-l", "_xyce.log", "_xyce.sp" };
  
  char *_init_xyce[sizeof (_cinit_xyce)/sizeof(_cinit_xyce[0])];
  int num = sizeof (_cinit_xyce)/sizeof (_cinit_xyce[0]);

  for (int i=0; i < num; i++) {
    _init_xyce[i] = Strdup (_cinit_xyce[i]);
  }
  xyce_initialize (&_xyce_ptr, num, _init_xyce);
  for (int i=0; i < num; i++) {
    FREE (_init_xyce[i]);
  }
#endif
}



XyceSim::XyceSim (ActSimCore *sim, Process *p) : ActSimObj (sim, p)
{
  XyceActInterface::addProcess (this);
  _si = sim->cursi();
}

XyceSim::~XyceSim()
{

}


int XyceSim::Step (int ev_type)
{
  /* hmm */
  return 1;
}

void XyceSim::computeFanout()
{

}


void XyceSim::setBool (int lid, int v)
{
  int off = getGlobalOffset (lid, 0);
  _sc->setBool (off, v);
}
