/* Keyword data.
   Copyright (C) 1989-1998, 2000, 2002 Free Software Foundation, Inc.
   Written by Douglas C. Schmidt <schmidt@ics.uci.edu>
   and Bruno Haible <bruno@clisp.org>.

   This file is part of GNU GPERF.

   GNU GPERF is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU GPERF is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Specification. */
#include "keyword.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "options.h"


/* --------------------------- KeywordExt class --------------------------- */

/* Sort a small set of 'unsigned char', base[0..len-1], in place.  */
static inline void sort_char_set (unsigned char *base, int len)
{
  /* Bubble sort is sufficient here.  */
  for (int i = 1; i < len; i++)
    {
      int j;
      unsigned char tmp;

      for (j = i, tmp = base[j]; j > 0 && tmp < base[j - 1]; j--)
        base[j] = base[j - 1];

      base[j] = tmp;
    }
}

/* Initialize selchars and selchars_length, and update occurrences.
   The hash function will be computed as
   asso_values[allchars[key_pos[0]]] + asso_values[allchars[key_pos[1]]] + ...
   We compute selchars as the multiset
     { allchars[key_pos[0]], allchars[key_pos[1]], ... }
   so that the hash function becomes
     asso_values[selchars[0]] + asso_values[selchars[1]] + ...
   Furthermore we sort the selchars array, to ease detection of duplicates
   later.
 */
void KeywordExt::init_selchars (int *occurrences)
{
  const char *k = _allchars;
  unsigned char *key_set =
    new unsigned char[(option[ALLCHARS] ? _allchars_length : option.get_max_keysig_size ())];
  unsigned char *ptr = key_set;

  if (option[ALLCHARS])
    /* Use all the character positions in the KEY.  */
    for (int i = _allchars_length; i > 0; k++, i--)
      {
        *ptr = static_cast<unsigned char>(*k);
        occurrences[*ptr]++;
        ptr++;
      }
  else
    /* Only use those character positions specified by the user.  */
    {
      /* Iterate through the list of key_positions, initializing occurrences
         table and selchars (via ptr).  */
      PositionIterator iter (option.get_key_positions ());

      for (int i; (i = iter.next ()) != PositionIterator::EOS; )
        {
          if (i == Positions::LASTCHAR)
            /* Special notation for last KEY position, i.e. '$'.  */
            *ptr = static_cast<unsigned char>(_allchars[_allchars_length - 1]);
          else if (i <= _allchars_length)
            /* Within range of KEY length, so we'll keep it.  */
            *ptr = static_cast<unsigned char>(_allchars[i - 1]);
          else
            /* Out of range of KEY length, so we'll just skip it.  */
            continue;
          occurrences[*ptr]++;
          ptr++;
        }

      /* Didn't get any hits and user doesn't want to consider the
         keylength, so there are essentially no usable hash positions!  */
      if (ptr == key_set && option[NOLENGTH])
        {
          fprintf (stderr, "Can't hash keyword %.*s with chosen key positions.\n",
                   _allchars_length, _allchars);
          exit (1);
        }
    }

  /* Sort the KEY_SET items alphabetically.  */
  sort_char_set (key_set, ptr - key_set);

  _selchars = key_set;
  _selchars_length = ptr - key_set;
}


/* ------------------------- Keyword_Factory class ------------------------- */

Keyword_Factory::Keyword_Factory ()
{
}

Keyword_Factory::~Keyword_Factory ()
{
}


#ifndef __OPTIMIZE__

#define INLINE /* not inline */
#include "keyword.icc"
#undef INLINE

#endif /* not defined __OPTIMIZE__ */
