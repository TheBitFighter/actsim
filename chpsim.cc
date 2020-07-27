/*************************************************************************
 *
 *  Copyright (c) 2020 Rajit Manohar
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
#include <simdes.h>
#include "chpsim.h"

class ChpSim;

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif


ChpSim::ChpSim (ChpSimGraph *g, act_chp_lang_t *c, ActSimCore *sim)
: ActSimObj (sim)
{
  /*
    Analyze the chp body to find out the maximum number of concurrent
    threads; those are the event types.
  */
  _npc = _max_program_counters (c);
  Assert (_npc >= 1, "What?");
  MALLOC (_pc, ChpSimGraph *, _npc);
  for (int i=0; i < _npc; i++) {
    _pc[i] = NULL;
  }

  Assert (_npc >= 1, "What?");
  _pc[0] = g;

  new Event (this, SIM_EV_MKTYPE (0,0), 10);
}

void ChpSim::Step (int ev_type)
{
  int pc = SIM_EV_TYPE (ev_type);
  int flag = SIM_EV_FLAGS (ev_type);
  int forceret = 0;
  expr_res v;
  int off;

  if (!_pc[pc]) {
    fatal_error ("NULL _pc? (pc=%d)\n", pc);
  }

  /*-- go forward through sim graph until there's some work --*/
  while (_pc[pc] && !_pc[pc]->stmt) {
    _pc[pc] = _pc[pc]->completed(pc, &forceret);
    if (forceret) return;
  }
  if (!_pc[pc]) return;
  
  chpsimstmt *stmt = _pc[pc]->stmt;
  forceret = 0;

  /*--- simulate statement until there's a blocking scenario ---*/
  printf ("[%8lu %d] <", CurTimeLo(), flag);
  name->Print (stdout);
  printf ("> ");
  switch (stmt->type) {
  case CHPSIM_ASSIGN:
    printf ("assign v[%d] := ", stmt->u.assign.var);
    v = exprEval (stmt->u.assign.e);
    printf ("  %d : %d", v.v, v.width);
    _pc[pc] = _pc[pc]->completed (pc, &forceret);
    if (stmt->u.assign.isbool) {
      off = getGlobalOffset (stmt->u.assign.var, 0);
#if 0      
      printf (" [glob=%d]", off);
#endif      
      _sc->setBool (off, v.v);
    }
    else {
      off = getGlobalOffset (stmt->u.assign.var, 1);
#if 0      
      printf (" [glob=%d]", off);
#endif
      _sc->setInt (off, v.v);
    }
    break;

  case CHPSIM_SEND:
    if (!flag) {
      listitem_t *li;
      li = list_first (stmt->u.send.el);
      if (li) {
	v = exprEval ((Expr *)list_value (li));
      }
      else {
	/* data-less */
	v.v = 0;
	v.width = 0;
      }
      printf ("send val=%d", v.v);
      if (varSend (pc, flag, stmt->u.send.chvar, v)) {
	/* blocked */
	forceret = 1;
      }
      else {
	_pc[pc] = _pc[pc]->completed (pc, &forceret);
      }
    }
    else {
      printf ("send done");
      if (!varSend (pc, flag, stmt->u.send.chvar, v)) {
	_pc[pc] = _pc[pc]->completed (pc, &forceret);
      }
    }
    break;

  case CHPSIM_RECV:
    {
      listitem_t *li;
      int id;
      int type;
      li = list_first (stmt->u.recv.vl);
      if (li) {
	type = (long)list_value (li);
	li = list_next (li);
	Assert (li, "What?");
	id = (long)list_value (li);
      }
      else {
	type = -1;
      }

      if (varRecv (pc, flag, stmt->u.recv.chvar, &v)) {
	printf ("recv blocked");
	forceret = 1;
      }
      else {
	printf ("recv got %d!", v.v);
	if (type != -1) {
	  if (type == 0) {
	    off = getGlobalOffset (id, 0);
#if 0	    
	    printf (" [glob=%d]", off);
#endif	    
	    _sc->setBool (off, v.v);
	  }
	  else {
	    off = getGlobalOffset (id, 1);
#if 0	    
	    printf (" [glob=%d]", off);
#endif	    
	    _sc->setInt (off, v.v);
	  }
	}
	_pc[pc] = _pc[pc]->completed (pc, &forceret);
      }
    }
    break;

  case CHPSIM_FUNC:
    printf ("func!");
    break;

  case CHPSIM_COND:
    {
      chpsimcond *gc;
      int cnt = 0;
      expr_res res;

      printf ("cond");
      
      gc = &stmt->u.c;
      while (gc) {
	if (!gc->g) {
	  res.v = 1;
	}
	else {
	  res = exprEval (gc->g);
	}
	if (res.v) {
	  _pc[pc] = _pc[pc]->all[cnt];
	  break;
	}
	cnt++;
	gc = gc->next;
      }
      /* all guards false */
      if (!gc) {
	if (_pc[pc]->next == NULL) {
	  /* selection: we just try again later */
	  break;
	}
	else {
	  /* loop: we're done! */
	  _pc[pc] = _pc[pc]->completed (pc, &forceret);
	}
      }
    }
    break;

  default:
    fatal_error ("What?");
    break;
  }
  printf ("\n");
  if (forceret) {
    return;
  }
  else {
    new Event (this, SIM_EV_MKTYPE (pc,0), 10);
  }
}

void ChpSim::varSet (int id, int type, expr_res v)
{
  int off = getGlobalOffset (id, type);
  if (type == 0) {
    _sc->setBool (off, v.v);
  }
  else if (type == 1) {
    _sc->setInt (off, v.v);
  }
  else {
    fatal_error ("Use channel send/recv!");
    /* channel! */
  }
}

/* returns 1 if blocked */
int ChpSim::varSend (int pc, int wakeup, int id, expr_res v)
{
  act_channel_state *c;
  int off = getGlobalOffset (id, 2);
  c = _sc->getChan (off);

  if (wakeup) {
    c->send_here = 0;
    return 0;
  }

  if (c->recv_here) {
    // blocked receive, because there was no data
    c->data = v.v;
    c->w->Notify (c->recv_here-1);
    c->send_here = 0;
    return 0;
  }
  else {
    // we need to wait for the receive to show up
    c->data2 = v.v;
    c->send_here = (pc+1);
    c->w->AddObject (this);
    return 1;
  }
}

/* returns 1 if blocked */
int ChpSim::varRecv (int pc, int wakeup, int id, expr_res *v)
{
  act_channel_state *c;
  int off = getGlobalOffset (id, 2);
  c = _sc->getChan (off);

  if (wakeup) {
    v->v = c->data;
    c->recv_here = 0;
    return 0;
  }
  if (c->send_here) {
    v->v = c->data2;
    c->w->Notify (c->send_here-1);
    c->recv_here = 0;
  }
  else {
    c->recv_here = (pc+1);
    c->w->AddObject (this);
    return 1;
  }
  return 0;
}


expr_res ChpSim::varEval (int id, int type)
{
  expr_res r;
  int off;

  off = getGlobalOffset (id, type == 3 ? 2 : type);
  if (type == 0) {
    r.width = 1;
    r.v = _sc->getBool (off);
  }
  else if (type == 1) {
    r.width = 32; /* XXX: need bit-widths */
    r.v = _sc->getInt (off);
  }
  else if (type == 2) {
    act_channel_state *c = _sc->getChan (off);
    if (c->send_here) {
      r.width = 32;
      r.v = c->data;
    }
    else {
      r.width = 0;
      r.v = 0;
    }
  }
  else {
    /* probe */
    act_channel_state *c = _sc->getChan (off);
    if (c->send_here || c->recv_here) {
      r.width = 1;
      r.v = 1;
    }
    else {
      r.width = 1;
      r.v = 0;
    }
  }
  return r;
}
  
expr_res ChpSim::exprEval (Expr *e)
{
  expr_res l, r;
  Assert (e, "What?!");

  switch (e->type) {

  case E_TRUE:
    l.v = 1;
    l.width = 1;
    return l;
    break;
    
  case E_FALSE:
    l.v = 0;
    l.width = 1;
    return l;
    break;
    
  case E_INT:
    l.v = e->u.v;
    {
      unsigned long x = e->u.v;
      l.width = 0;
      while (x) {
	x = x >> 1;
	l.width++;
      }
      if (l.width == 0) {
	l.width = 1;
      }
    }
    return l;
    break;

  case E_REAL:
    fatal_error ("No real expressions permitted in CHP!");
    return l;
    break;

    /* binary */
  case E_AND:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = MAX(l.width, r.width);
    l.v = l.v & r.v;
    break;

  case E_OR:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = MAX(l.width, r.width);
    l.v = l.v | r.v;
    break;

  case E_PLUS:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1 + MAX(l.width, r.width);
    l.v = l.v + r.v;
    break;

  case E_MINUS:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1 + MAX(l.width, r.width);
    l.v = l.v - r.v;
    break;

  case E_MULT:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = l.width + r.width;
    l.v = l.v * r.v;
    break;
    
  case E_DIV:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    if (r.v == 0) {
      warning("Division by zero");
    }
    l.v = l.v / r.v;
    break;
      
  case E_MOD:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = r.width;
    l.v = l.v % r.v;
    break;
    
  case E_LSL:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = l.width + (1 << r.width) - 1;
    l.v = l.v << r.v;
    break;
    
  case E_LSR:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.v = l.v >> r.v;
    break;
    
  case E_ASR:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    if ((l.v >> (l.width-1)) & 1) {
      l.v = l.v >> r.v;
      l.v = l.v | (((1U << r.v)-1) << (l.width-r.v));
    }
    else {
      l.v = l.v >> r.v;
    }
    break;
    
  case E_XOR:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = MAX(l.width, r.width);
    l.v = l.v ^ r.v;
    break;
    
  case E_LT:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v < r.v) ? 1 : 0;
    break;
    
  case E_GT:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v > r.v) ? 1 : 0;
    break;

  case E_LE:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v <= r.v) ? 1 : 0;
    break;
    
  case E_GE:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v >= r.v) ? 1 : 0;
    break;
    
  case E_EQ:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v == r.v) ? 1 : 0;
    break;
    
  case E_NE:
    l = exprEval (e->u.e.l);
    r = exprEval (e->u.e.r);
    l.width = 1;
    l.v = (l.v != r.v) ? 1 : 0;
    break;
    
  case E_NOT:
    l = exprEval (e->u.e.l);
    l.v = !l.v;
    break;
    
  case E_UMINUS:
    l = exprEval (e->u.e.l);
    l.v = (1 << l.width) - l.v;
    break;

  case E_COMPLEMENT:
    l = exprEval (e->u.e.l);
    l.v = ((1 << l.width)-1) - l.v;
    break;

  case E_QUERY:
    l = exprEval (e->u.e.l);
    if (l.v) {
      l = exprEval (e->u.e.r->u.e.l);
    }
    else {
      l = exprEval (e->u.e.r->u.e.r);
    }
    break;

  case E_COLON:
  case E_COMMA:
    fatal_error ("Should have been handled elsewhere");
    break;

  case E_CONCAT:
    l.v = 0;
    l.width = 0;
    do {
      r = exprEval (e->u.e.l);
      l.width += r.width;
      l.v = (l.v << r.width) | r.v;
      e = e->u.e.r;
    } while (e);
    break;

  case E_BITFIELD:
    l = varEval (e->u.v, 1); 
    r = exprEval (e->u.e.r->u.e.r);
    if (r.v > l.width) {
      warning ("Bit-width is less than the width specifier");
    }
    l.width = r.v;
    r = exprEval (e->u.e.r->u.e.l);
    if (r.v > l.width) {
      warning ("Bit-width extraction results in no bits; setting to 0?");
    }
    l.v = l.v >> r.v;
    l.width = l.width - r.v + 1;
    if (l.width <= 0) {
      l.width = 1;
      l.v = 0;
    }
    else {
      l.v = l.v >> r.v;
    }
    break;

  case E_CHP_VARBOOL:
    l = varEval (e->u.v, 0);
    break;
  case E_CHP_VARINT:
    l = varEval (e->u.v, 1);
    break;
  case E_CHP_VARCHAN:
    l = varEval (e->u.v, 2);
    break;

  case E_VAR:
    fatal_error ("VARS?!");
    break;

  case E_PROBE:
    l = varEval (e->u.v, 3);
    break;

  case E_BUILTIN_BOOL:
    l = exprEval (e->u.e.l);
    if (l.v) {
      l.v = 1;
      l.width = 1;
    }
    else {
      l.v = 0;
      l.width = 1;
    }
    break;
    
  case E_BUILTIN_INT:
    l = exprEval (e->u.e.l);
    if (e->u.e.r) {
      r = exprEval (e->u.e.r);
    }
    else {
      r.v = 1;
    }
    l.width = r.v;
    l.v = l.v & ((1 << r.v) - 1);
    break;

#if 0
  case E_FUNCTION:
    ret->u.fn.s = e->u.fn.s;
    ret->u.fn.r = NULL;
    {
      Expr *tmp = NULL;
      e = e->u.fn.r;
      while (e) {
	if (!tmp) {
	  NEW (tmp, Expr);
	  ret->u.fn.r = tmp;
	}
	else {
	  NEW (tmp->u.e.r, Expr);
	  tmp = tmp->u.e.r;
	}
	tmp->type = e->type;
	tmp->u.e.l = expr_to_chp_expr (e->u.e.l, s);
	e = e->u.e.r;
      }
    }
    break;
#endif

  case E_FUNCTION:
  case E_SELF:
  default:
    l.v = 0;
    l.width = 1;
    fatal_error ("Unknown expression type %d\n", e->type);
    break;
  }
  if (l.width <= 0) {
    warning ("Negative width?");
    l.width = 1;
    l.v = 0;
  }
  return l;
}

int ChpSim::_max_program_counters (act_chp_lang_t *c)
{
  int ret, val;

  if (!c) return 1;
  
  switch (c->type) {
  case ACT_CHP_SEMI:
    ret = 1;
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      act_chp_lang_t *t = (act_chp_lang_t *) list_value (li);
      val = _max_program_counters (t);
      if (val > ret) {
	ret = val;
      }
    }
    break;

  case ACT_CHP_COMMA:
    ret = 0;
    for (listitem_t *li = list_first (c->u.semi_comma.cmd);
	 li; li = list_next (li)) {
      act_chp_lang_t *t = (act_chp_lang_t *) list_value (li);
      ret += _max_program_counters (t);
    }
    break;

  case ACT_CHP_SELECT:
  case ACT_CHP_SELECT_NONDET:
  case ACT_CHP_LOOP:
  case ACT_CHP_DOLOOP:
    ret = 0;
    for (act_chp_gc_t *gc = c->u.gc; gc; gc = gc->next) {
      val = _max_program_counters (gc->s);
      if (val > ret) {
	ret = val;
      }
    }
    break;

  case ACT_CHP_SKIP:
  case ACT_CHP_ASSIGN:
  case ACT_CHP_SEND:
  case ACT_CHP_RECV:
  case ACT_CHP_FUNC:
    ret = 1;
    break;

  default:
    fatal_error ("Unknown chp type %d\n", c->type);
    break;
  }
  return ret;
}
