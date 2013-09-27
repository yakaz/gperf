#ifndef output_javascript_h
#define output_javascript_h 1

#include "output.h"

class OutputJavascript : public Output
{
public:
  /* Constructor. */
  OutputJavascript (KeywordExt_List *head,
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
                    const int *asso_values);

  /* Generates the hash function and the key word recognizer function.  */
  void output ();

protected:

  /* Outputs the maximum and minimum hash values etc.  */
  void                  output_constants (const char *indent) const;

  /* Generates a JavaScript expression for an asso_values[] reference.  */
  void                  output_asso_values_ref (int pos) const;

  /* Generates JavaScript code for the hash function that returns the
     proper encoding for each keyword.  */
  void                  output_hash_function () const;

  /* Prints out the array containing the keywords for the hash function.  */
  void                  output_keyword_table () const;

  /* Generate all the tables needed for the lookup function.  */
  void                  output_lookup_tables () const;

  /* Generates JavaScript code to perform the keyword lookup.  */
  void                  output_lookup_function_body () const;

  /* Generates JavaScript code for the lookup function.  */
  void                  output_lookup_function () const;
};

#endif
