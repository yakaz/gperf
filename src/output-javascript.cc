#include <stdio.h>

/* Specification. */
#include "output-javascript.h"

#include <assert.h> /* defines assert() */
#include <ctype.h>  /* declares isprint() */
#include <stdio.h>
#include "options.h"
#include "version.h"

OutputJavascript::OutputJavascript (KeywordExt_List *head,
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
OutputJavascript::output_constants (const char *indent) const
{
  const char *prefix = option.get_constants_prefix ();

  if (option[GLOBAL])
    {
      printf (
          "%s%sTOTAL_KEYWORDS:  %d,\n"
          "%s%sMIN_WORD_LENGTH: %d,\n"
          "%s%sMAX_WORD_LENGTH: %d,\n"
          "%s%sMIN_HASH_VALUE:  %d,\n"
          "%s%sMAX_HASH_VALUE:  %d,\n"
          "\n",
          indent, prefix, _total_keys,
          indent, prefix, _min_key_len,
          indent, prefix, _max_key_len,
          indent, prefix, _min_hash_value,
          indent, prefix, _max_hash_value);
    }
  else
    {
      printf (
          "%svar %sTOTAL_KEYWORDS  = %d,\n"
          "%svar %sMIN_WORD_LENGTH = %d,\n"
          "%svar %sMAX_WORD_LENGTH = %d,\n"
          "%svar %sMIN_HASH_VALUE  = %d,\n"
          "%svar %sMAX_HASH_VALUE  = %d,\n"
          "\n",
          indent, prefix, _total_keys,
          indent, prefix, _min_key_len,
          indent, prefix, _max_key_len,
          indent, prefix, _min_hash_value,
          indent, prefix, _max_hash_value);
    }
}

/* ------------------------------------------------------------------------- */

/* Generates a JavaScript expression for an asso_values[] reference.  */

void
OutputJavascript::output_asso_values_ref (int pos) const
{
  printf ("asso_values[");
  /* Always cast to unsigned char.  This is necessary when the alpha_inc
     is nonzero, and also avoids a gcc warning "subscript has type 'char'".  */
  if (pos == Positions::LASTCHAR)
    printf ("str.charCodeAt(str.length - 1)");
  else
    {
      printf ("str.charCodeAt(%d)", pos);
      if (_alpha_inc[pos])
        printf ("+%u", _alpha_inc[pos]);
    }
  printf ("]");
}

void
OutputJavascript::output_hash_function () const
{
  printf ("  %s: function(str) {\n", option.get_hash_name ());

  /* First the asso_values array.  */
  if (_key_positions.get_size() > 0)
    {
      printf ("    var asso_values = [");

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
            printf ("\n     ");
          printf ("%*d", field_width, _asso_values[count]);
        }

      printf ("\n"
              "    ];\n");
    }

  if (_key_positions.get_size() == 0)
    {
      /* Trivial case: No key positions at all.  */
      printf ("    return %s;\n",
              _hash_includes_len ? "str.length" : "0");
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
                  _hash_includes_len ? "str.length + " : "");

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

          printf (";\n");
        }
      else
        {
          /* We've got to use the correct, but brute force, technique.  */
          printf ("    var hval = %s;\n\n",
                  _hash_includes_len ? "str.length" : "0");

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

                  printf ("    if (hval >= %d)\n", key_pos + 1);
                  printf ("      hval += ");
                  output_asso_values_ref (key_pos);
                  printf (";\n");

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
          printf (";\n");
        }
    }

  printf ("  },\n");
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

/* Outputs a #line directive, referring to the given line number.  */

static void
output_line_directive (unsigned int lineno)
{
  const char *file_name = option.get_input_file_name ();
  if (file_name != NULL)
    {
      printf ("#line %u ", lineno);
      output_string (file_name, strlen (file_name));
      printf ("\n");
    }
}

/* ------------------------------------------------------------------------- */

static void
output_keyword_entry (KeywordExt *temp, int stringpool_index, const char *indent)
{
  printf ("%s  ", indent);
  output_string (temp->_allchars, temp->_allchars_length);
  if (option[DEBUG])
    printf (" /* hash value = %d, index = %d */",
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
          printf ("%s  ", indent);
        }
      else
        {
          if (i > 0)
            printf (", ");
        }
      if (option[NULLSTRINGS])
        printf ("0");
      else
        printf ("\"\"");
      column++;
    }
}

/* Prints out the array containing the keywords for the hash function.  */

void
OutputJavascript::output_keyword_table () const
{
  const char *indent  = option[GLOBAL] ? "" : "    ";
  int index;
  KeywordExt_List *temp;

  printf ("%svar %s = [\n",
          indent, option.get_wordlist_name ());

  /* Generate an array of reserved words at appropriate locations.  */

  for (temp = _head, index = 0; temp; temp = temp->rest())
    {
      KeywordExt *keyword = temp->first();

      /* If generating a switch statement, and there is no user defined type,
         we generate non-duplicates directly in the code.  Only duplicates go
         into the table.  */
      if (option[SWITCH] && !keyword->_duplicate_link)
        continue;

      if (index > 0)
        printf (",\n");

      if (index < keyword->_hash_value && !option[SWITCH])
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

  printf ("%s];\n\n", indent);
}

/* Generate all the tables needed for the lookup function.  */

void
OutputJavascript::output_lookup_tables () const
{
  if (!option[SWITCH])
    {
      /* Use the lookup table, in place of switch.  */
      output_keyword_table ();
      //output_lookup_array ();
    }
}

/* ------------------------------------------------------------------------- */

/* Output a single switch case (including duplicates).  Advance list.  */

static KeywordExt_List *
output_switch_case (KeywordExt_List *list, int indent, int *jumps_away)
{
  if (option[DEBUG])
    printf ("%*s/* hash value = %4d, keyword = \"%.*s\" */\n",
            indent, "", list->first()->_hash_value, list->first()->_allchars_length, list->first()->_allchars);

  printf ("%*sresword = ",
          indent, "");
  output_string (list->first()->_allchars, list->first()->_allchars_length);
  printf (";\n");
  printf ("%*sbreak;\n",
          indent, "");
  *jumps_away = 1;

  return list->rest();
}

/* Output a total of size cases, grouped into num_switches switch statements,
   where 0 < num_switches <= size.  */

static void
output_switches (KeywordExt_List *list, int num_switches, int size, int min_hash_value, int max_hash_value, int indent)
{
  if (option[DEBUG])
    printf ("%*s/* know %d <= key <= %d, contains %d cases */\n",
            indent, "", min_hash_value, max_hash_value, size);

  if (num_switches > 1)
    {
      int part1 = num_switches / 2;
      int part2 = num_switches - part1;
      int size1 = static_cast<int>(static_cast<double>(size) / static_cast<double>(num_switches) * static_cast<double>(part1) + 0.5);
      int size2 = size - size1;

      KeywordExt_List *temp = list;
      for (int count = size1; count > 0; count--)
        temp = temp->rest();

      printf ("%*sif (key < %d) {\n",
              indent, "", temp->first()->_hash_value);

      output_switches (list, part1, size1, min_hash_value, temp->first()->_hash_value-1, indent+2);

      printf ("%*s} else {\n",
              indent, "");

      output_switches (temp, part2, size2, temp->first()->_hash_value, max_hash_value, indent+2);

      printf ("%*s}\n",
              indent, "");
    }
  else
    {
      /* Output a single switch.  */
      int lowest_case_value = list->first()->_hash_value;
      if (size == 1)
        {
          int jumps_away = 0;
          assert (min_hash_value <= lowest_case_value);
          assert (lowest_case_value <= max_hash_value);
          if (min_hash_value == max_hash_value)
            output_switch_case (list, indent, &jumps_away);
          else
            {
              printf ("%*sif (key == %d) {\n",
                      indent, "", lowest_case_value);
              output_switch_case (list, indent+2, &jumps_away);
              printf ("%*s}\n",
                      indent, "");
            }
        }
      else
        {
          if (lowest_case_value == 0)
            printf ("%*sswitch (key) {\n", indent, "");
          else
            printf ("%*sswitch (key - %d) {\n", indent, "", lowest_case_value);
          for (; size > 0; size--)
            {
              int jumps_away = 0;
              printf ("%*scase %d:\n",
                      indent, "", list->first()->_hash_value - lowest_case_value);
              list = output_switch_case (list, indent+2, &jumps_away);
              if (!jumps_away)
                printf ("%*s  break;\n",
                        indent, "");
            }
          printf ("%*s}\n",
                  indent, "");
        }
    }
}

/* Generates Javascript code to perform the keyword lookup.  */

void
OutputJavascript::output_lookup_function_body () const
{
  printf ("    if (str.length <= %sMAX_WORD_LENGTH && str.length >= %sMIN_WORD_LENGTH) {\n"
          "      var key = %s(str);\n\n",
          option.get_constants_prefix (), option.get_constants_prefix (),
          option.get_hash_name ());

  if (option[SWITCH])
    {
      int switch_size = num_hash_values ();
      int num_switches = option.get_total_switches ();
      if (num_switches > switch_size)
        num_switches = switch_size;

      printf ("      if (key <= %sMAX_HASH_VALUE && key >= %sMIN_HASH_VALUE) {\n"
              "        var resword;\n\n",
              option.get_constants_prefix (), option.get_constants_prefix ());

      output_switches (_head, num_switches, switch_size, _min_hash_value, _max_hash_value, 6);

      printf ("        return (resword && str == resword);\n");
      printf ("      }\n");
    }
  else
    {
      printf ("      if (key <= %sMAX_HASH_VALUE && key >= 0) {\n",
              option.get_constants_prefix ());

      int indent = 6;
      printf ("%*s  var s = %s[key]",
              indent, "", option.get_wordlist_name ());

      printf (";\n\n"
              "%*s  if (",
              indent, "");
      if (option[NULLSTRINGS])
        printf ("s && ");
      printf ("str == s)\n"
              "%*s    return true;\n",
              indent, "");
      printf ("%*s}\n",
              indent, "");
    }
  printf ("    }\n"
          "    return false;\n");
}

/* Generates Javascript code for the lookup function.  */

void
OutputJavascript::output_lookup_function () const
{
  printf ("\n  %s: function(str) {\n", option.get_function_name ());

  if (!option[GLOBAL])
      output_constants ("    ");

  if (!option[GLOBAL])
    output_lookup_tables ();

  output_lookup_function_body ();

  printf ("  },\n");
}

void
OutputJavascript::output ()
{

  compute_min_max ();

  printf (
      "/* JavaScript code produced by gperf %s */\n"
      "/* vim:set ft=javascript: */\n",
      version_string);
  option.print_options ();
  printf ("\n");
  if (!option[POSITIONS])
    {
      printf ("/* Computed positions: -k'");
      _key_positions.print();
      printf ("' */\n");
    }
  printf ("\n");

  printf ("var %s = {\n", option.get_class_name ());

  if (option[GLOBAL])
    output_constants ("  ");

  output_hash_function ();

  if (!option[NOLOOKUPFUNC]) {
    if (option[GLOBAL])
      output_lookup_tables ();

    output_lookup_function ();
  }

  printf ("};\n");

  fflush (stdout);
}
