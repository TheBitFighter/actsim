/*************************************************************************
 *
 *  This file is part of the ACT library
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
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <act/act.h>
#include <act/passes.h>
#include "config.h"
#include "actsim.h"


static void usage (char *name)
{
  fprintf (stderr, "Usage: %s <actfile> <process>\n", name);
  exit (1);
}


int main (int argc, char **argv)
{
  Act *a;
  char *proc;

  /* initialize ACT library */
  Act::Init (&argc, &argv);

  /* some usage check */
  if (argc != 3) {
    usage (argv[0]);
  }

  /* read in the ACT file */
  a = new Act (argv[1]);

  /* expand it */
  a->Expand ();

  /* map to cells: these get characterized */
  //ActCellPass *cp = new ActCellPass (a);
  //cp->run();
 
  /* find the process specified on the command line */
  Process *p = a->findProcess (argv[2]);

  if (!p) {
    fatal_error ("Could not find process `%s' in file `%s'", argv[2], argv[1]);
  }

  if (!p->isExpanded()) {
    fatal_error ("Process `%s' is not expanded.", argv[2]);
  }

  /* do stuff here */
  ActStatePass *sp = new ActStatePass (a);
  sp->run (p);
  
  ActSim *sim = new ActSim (p);

  sim->runSim (NULL);

  return 0;
}
