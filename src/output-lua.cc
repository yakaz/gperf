#include <stdio.h>

/* Specification. */
#include "output-lua.h"

#include <assert.h> /* defines assert() */
#include <ctype.h>  /* declares isprint() */
#include <stdio.h>
#include "options.h"
#include "version.h"

OutputLua::OutputLua (KeywordExt_List *head,
                      const char *struct_decl,
                      unsigned int struct_decl_lineno,
                      const char *return_type,
                      const char *struct_tag,
                      const char *verbatim_declarations,
                      const char *verbatim_declarations_end,
                      unsigned int verbatim_declarations_lineno,
                      const char *verbatim_code,
                      const char *verbatim_code_end,
                      unsigned int verbatim_code_lineno,
                      bool charset_dependent,
                      int total_keys,
                      int max_key_len, int min_key_len,
                      bool hash_includes_len,
                      const Positions& positions,
                      const unsigned int *alpha_inc,
                      int total_duplicates,
                      unsigned int alpha_size,
                      const int *asso_values)
  : Output(head, struct_decl, struct_decl_lineno, return_type, struct_tag,
      verbatim_declarations, verbatim_declarations_end,
      verbatim_declarations_lineno, verbatim_code, verbatim_code_end,
      verbatim_code_lineno, charset_dependent, total_keys, max_key_len,
      min_key_len, hash_includes_len, positions, alpha_inc, total_duplicates,
      alpha_size, asso_values)
{
}

/* Outputs the maximum and minimum hash values etc.  */

void
OutputLua::output_constants (const char *indent) const
{
  const char *prefix = option.get_constants_prefix ();

  if (option[GLOBAL])
    {
      printf (
          "\n"
          "%s%s.%sTOTAL_KEYWORDS  = %d\n"
          "%s%s.%sMIN_WORD_LENGTH = %d\n"
          "%s%s.%sMAX_WORD_LENGTH = %d\n"
          "%s%s.%sMIN_HASH_VALUE  = %d\n"
          "%s%s.%sMAX_HASH_VALUE  = %d\n",
          indent, option.get_class_name (), prefix, _total_keys,
          indent, option.get_class_name (), prefix, _min_key_len,
          indent, option.get_class_name (), prefix, _max_key_len,
          indent, option.get_class_name (), prefix, _min_hash_value,
          indent, option.get_class_name (), prefix, _max_hash_value);
    }
  else
    {
      printf (
          "%slocal %sTOTAL_KEYWORDS  = %d\n"
          "%slocal %sMIN_WORD_LENGTH = %d\n"
          "%slocal %sMAX_WORD_LENGTH = %d\n"
          "%slocal %sMIN_HASH_VALUE  = %d\n"
          "%slocal %sMAX_HASH_VALUE  = %d\n"
          "\n",
          indent, prefix, _total_keys,
          indent, prefix, _min_key_len,
          indent, prefix, _max_key_len,
          indent, prefix, _min_hash_value,
          indent, prefix, _max_hash_value);
    }
}

/* ------------------------------------------------------------------------- */

/* Generates a Lua expression for an asso_values[] reference.  */

void
OutputLua::output_asso_values_ref (int pos) const
{
  printf ("asso_values[");
  /* Always cast to unsigned char.  This is necessary when the alpha_inc
     is nonzero, and also avoids a gcc warning "subscript has type 'char'".  */
  if (pos == Positions::LASTCHAR)
    printf ("str:byte(-1)");
  else
    {
      printf ("str:byte(%d + 1)", pos);
      if (_alpha_inc[pos])
        printf ("+%u", _alpha_inc[pos]);
    }
  printf (" + 1]");
}

void
OutputLua::output_hash_function () const
{
  printf ("\nfunction %s.%s(str)\n",
      option.get_class_name (), option.get_hash_name ());

  /* First the asso_values array.  */
  if (_key_positions.get_size() > 0)
    {
      printf ("    local asso_values = {");

      const int columns = 10;

      /* Calculate maximum number of digits required for MAX_HASH_VALUE.  */
      int field_width = 2;
      for (int trunc = _max_hash_value; (trunc /= 10) > 0;)
        field_width++;

      for (unsigned int count = 0; count < _alpha_size; count++)
        {
          if (count > 0)
            printf (",");
          if ((count % columns) == 0)
            printf ("\n       ");
          printf ("%*d", field_width, _asso_values[count]);
        }

      printf ("\n"
              "    }\n");
    }

  if (_key_positions.get_size() == 0)
    {
      /* Trivial case: No key positions at all.  */
      printf ("    return %s\n",
              _hash_includes_len ? "str:len()" : "0");
    }
  else
    {
      /* Iterate through the key positions.  Remember that Positions::sort()
         has sorted them in decreasing order, with Positions::LASTCHAR coming
         last.  */
      PositionIterator iter = _key_positions.iterator(_max_key_len);
      int key_pos;

      /* Get the highest key position.  */
      key_pos = iter.next ();

      if (key_pos == Positions::LASTCHAR || key_pos < _min_key_len)
        {
          /* We can perform additional optimizations here:
             Write it out as a single expression. Note that the values
             are added as 'int's even though the asso_values array may
             contain 'unsigned char's or 'unsigned short's.  */

          printf ("    return %s",
                  _hash_includes_len ? "str:len() + " : "");

          if (_key_positions.get_size() == 2
              && _key_positions[0] == 0
              && _key_positions[1] == Positions::LASTCHAR)
            /* Optimize special case of "-k 1,$".  */
            {
              output_asso_values_ref (Positions::LASTCHAR);
              printf (" + ");
              output_asso_values_ref (0);
            }
          else
            {
              for (; key_pos != Positions::LASTCHAR; )
                {
                  output_asso_values_ref (key_pos);
                  if ((key_pos = iter.next ()) != PositionIterator::EOS)
                    printf (" + ");
                  else
                    break;
                }

              if (key_pos == Positions::LASTCHAR)
                output_asso_values_ref (Positions::LASTCHAR);
            }

          printf ("\n");
        }
      else
        {
          /* We've got to use the correct, but brute force, technique.  */
          printf ("    local hval = %s\n\n",
                  _hash_includes_len ? "str:len()" : "0");

          while (key_pos != Positions::LASTCHAR && key_pos >= _max_key_len)
            if ((key_pos = iter.next ()) == PositionIterator::EOS)
              break;

          if (key_pos != PositionIterator::EOS && key_pos != Positions::LASTCHAR)
            {
              int i = key_pos;
              do
                {
                  for ( ; i > key_pos; i--)
                    ;

                  printf ("    if (hval >= %d) then\n", key_pos + 1);
                  printf ("        hval = hval + ");
                  output_asso_values_ref (key_pos);
                  printf ("\n");
                  printf ("    end\n");

                  key_pos = iter.next ();
                }
              while (key_pos != PositionIterator::EOS && key_pos != Positions::LASTCHAR);
            }

          printf ("    return hval");
          if (key_pos == Positions::LASTCHAR)
            {
              printf (" + ");
              output_asso_values_ref (Positions::LASTCHAR);
            }
          printf ("\n");
        }
    }

  printf ("end\n");
}

/* ------------------------------------------------------------------------- */

/* Outputs a keyword, as a string: enclosed in double quotes, escaping
   backslashes, double quote and unprintable characters.  */

static void
output_string (const char *key, int len)
{
  putchar ('"');
  for (; len > 0; len--)
    {
      unsigned char c = static_cast<unsigned char>(*key++);
      if (isprint (c))
        {
          if (c == '"' || c == '\\')
            putchar ('\\');
          putchar (c);
        }
      else
        {
          /* Use octal escapes, not hexadecimal escapes, because some old
             C compilers didn't understand hexadecimal escapes, and because
             hexadecimal escapes are not limited to 2 digits, thus needing
             special care if the following character happens to be a digit.  */
          putchar ('\\');
          putchar ('0' + ((c >> 6) & 7));
          putchar ('0' + ((c >> 3) & 7));
          putchar ('0' + (c & 7));
        }
    }
  putchar ('"');
}

/* ------------------------------------------------------------------------- */

static void
output_keyword_entry (KeywordExt *temp, int stringpool_index, const char *indent)
{
  printf ("%s    ", indent);
  output_string (temp->_allchars, temp->_allchars_length);
  if (option[DEBUG])
    printf (" -- hash value = %d, index = %d",
            temp->_hash_value, temp->_final_index);
}

static void
output_keyword_blank_entries (int count, const char *indent)
{
  int columns;
  columns = (option[NULLSTRINGS] ? 4 : 9);

  int column = 0;
  for (int i = 0; i < count; i++)
    {
      if ((column % columns) == 0)
        {
          if (i > 0)
            printf (",\n");
          printf ("%s    ", indent);
        }
      else
        {
          if (i > 0)
            printf (", ");
        }
      if (option[NULLSTRINGS])
        printf ("nil");
      else
        printf ("\"\"");
      column++;
    }
}

/* Prints out the array containing the keywords for the hash function.  */

void
OutputLua::output_keyword_table () const
{
  const char *indent  = option[GLOBAL] ? "" : "    ";
  int index;
  KeywordExt_List *temp;

  if (option[GLOBAL])
    printf ("\n%s%s.%s = {\n",
            indent, option.get_class_name (), option.get_wordlist_name ());
  else
    printf ("%slocal %s = {\n",
            indent, option.get_wordlist_name ());

  /* Generate an array of reserved words at appropriate locations.  */

  for (temp = _head, index = 0; temp; temp = temp->rest())
    {
      KeywordExt *keyword = temp->first();

      if (index > 0)
        printf (",\n");

      if (index < keyword->_hash_value)
        {
          /* Some blank entries.  */
          output_keyword_blank_entries (keyword->_hash_value - index, indent);
          printf (",\n");
          index = keyword->_hash_value;
        }

      keyword->_final_index = index;

      output_keyword_entry (keyword, index, indent);

      index++;
    }
  if (index > 0)
    printf ("\n");

  printf ("%s}\n", indent);
}

/* Generate all the tables needed for the lookup function.  */

void
OutputLua::output_lookup_tables () const
{
  /* Use the lookup table, in place of switch.  */
  output_keyword_table ();
  //output_lookup_array ();
}

/* ------------------------------------------------------------------------- */

/* Generates Lua code to perform the keyword lookup.  */

void
OutputLua::output_lookup_function_body () const
{
  if (option[GLOBAL])
    {
      printf ("    if (str:len() <= %s.%sMAX_WORD_LENGTH and str:len() >= %s.%sMIN_WORD_LENGTH) then\n"
          "        local key = %s.%s(str)\n\n",
          option.get_class_name (), option.get_constants_prefix (),
          option.get_class_name (), option.get_constants_prefix (),
          option.get_class_name (), option.get_hash_name ());

      printf ("        if (key <= %s.%sMAX_HASH_VALUE and key >= 0) then\n",
          option.get_class_name (), option.get_constants_prefix ());
    }
  else
    {
      printf ("    if (str:len() <= %sMAX_WORD_LENGTH and str:len() >= %sMIN_WORD_LENGTH) then\n"
          "        local key = %s.%s(str)\n\n",
          option.get_constants_prefix (), option.get_constants_prefix (),
          option.get_class_name (), option.get_hash_name ());

      printf ("        if (key <= %sMAX_HASH_VALUE and key >= 0) then\n",
          option.get_constants_prefix ());
    }

  int indent = 8;
  if (option[GLOBAL])
    printf ("%*s    local s = %s.%s[key + 1]",
            indent, "", option.get_class_name (), option.get_wordlist_name ());
  else
    printf ("%*s    local s = %s[key + 1]",
            indent, "", option.get_wordlist_name ());

  printf ("\n\n"
          "%*s    if (",
          indent, "");
  if (option[NULLSTRINGS])
    printf ("s && ");
  printf ("str == s) then\n"
          "%*s        return true\n",
          indent, "");
  printf ("%*s    end\n", indent, "");
  printf ("%*send\n",
          indent, "");

  printf ("    end\n"
          "    return false\n");
}

/* Generates Lua code for the lookup function.  */

void
OutputLua::output_lookup_function () const
{
  printf ("\nfunction %s.%s(str)\n",
      option.get_class_name (), option.get_function_name ());

  if (!option[GLOBAL])
      output_constants ("    ");

  if (!option[GLOBAL])
    {
      output_lookup_tables ();
      printf ("\n");
    }

  output_lookup_function_body ();

  printf ("end\n");
}

void
OutputLua::output ()
{
  compute_min_max ();

  printf (
      "-- Lua code produced by gperf %s\n"
      "-- vim:set ft=lua:\n",
      version_string);
  printf ("-- ");
  option.print_options ();
  printf ("\n");
  if (!option[POSITIONS])
    {
      printf ("-- Computed positions: -k'");
      _key_positions.print();
      printf ("'\n");
    }
  printf ("\n");

  printf ("local %s = {}\n", option.get_class_name ());

  if (option[GLOBAL])
    output_constants ("");

  output_hash_function ();

  if (!option[NOLOOKUPFUNC]) {
    if (option[GLOBAL])
      output_lookup_tables ();

    output_lookup_function ();
  }

  printf ("\nreturn %s\n", option.get_class_name ());

  fflush (stdout);
}
