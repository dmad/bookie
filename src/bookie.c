/* -*- mode: C; c-file-style: "gnu" -*- */
/*
 * Copyright (C) 2011 Dirk Dierckx <dirk.dierckx@gmail.com>
 *
 * This file is part of bookie.
 *
 * bookie is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * bookie is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bookie.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdlib.h> /* alloc, realloc, free */
#include <string.h> /* strcpy, strlen, memset */
#include <ctype.h> /* isspace */
#include <stdio.h>

#include "arguments.h"

struct arguments 
{
  bool list_details;  
  bool list_by_account;
  bool list_by_date;
  bool list_total;

  bool invert_amounts;

  /* filter */
  char account[4];
  char from_date[9], to_date[9];

  FILE *input;
};

struct entry 
{
  char account[4];
  char date[9];
  char desc[80];
  float amount;
};

size_t entries_allocated;
size_t entries_used;
struct entry *entries;

static
void
_print_usage_header (const char *command)
{
  printf ("Usage: %s [OPTION]... [FILE]\n"
	  "Read a list and retrieve some data out of it.\n",
	  command);
}

static
void
_print_version ()
{
  printf ("%s %s\n"
	  "\n"
	  "Copyright 2011 by Dirk Dierckx <dirk.dierckx@gmail.com>\n"
	  "This is free software; see the source for copying conditions.\n"
	  "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
	  "FOR A PARTICULAR PURPOSE.\n",
	  PACKAGE, VERSION);
}

static
bool
_process_option (struct arguments_definition *def,
		 int opt,
		 const char *optarg,
		 int argc,
		 char *argv[])
{
  struct arguments *args = def->user_data;
  bool go_on = true;
  
  switch (opt) {
  case '?': /* invalid option */
    go_on = false;
    break;
  case ':': /* invalid argument */
  case 'h':
    print_usage (def, argv[0]);
    go_on = false;
    break;
  case 'V':
    _print_version ();
    go_on = false;
    break;
  case 500:
    args->list_details = true;
    break;
  case 'A':
    args->list_by_account = true;
    break;
  case 'D':
    args->list_by_date = true;
    break;
  case 'T':
    args->list_total = true;
    break;
  case 'a':
    strncpy (args->account, optarg, 4);
    args->account[3] = '\0';
    break;
  case 'f':
    strncpy (args->from_date, optarg, 9);
    args->from_date[8] = '\0';
    break;
  case 't':
    strncpy (args->to_date, optarg, 9);
    args->to_date[8] = '\0';
    break;
  case 'd':
    strncpy (args->from_date, optarg, 9);
    args->from_date[8] = '\0';
    strcpy (args->to_date, args->from_date);
    break;
  case 'i':
    args->invert_amounts = true;
    break;
  default: /* unhandled option */
    fprintf (stderr, "Unhandled option %d\n", opt);
    go_on = false;
    break;
  }
  
  return go_on;
}

static
bool
_process_non_options (struct arguments_definition *def,
		      int optind,
		      int argc,
		      char *argv[])
{
  struct arguments *args = def->user_data;
  bool go_on = true;
  
  for (int i = optind;go_on && i < argc;++i) {
    go_on = false;
    
    if (NULL == args->input) {
      if (0 != strcmp ("-", argv[optind]))
	args->input = fopen (argv[optind], "r");
      else
	args->input = stdin;

      go_on = NULL != args->input;
    }
  }

  return go_on;
}

static
bool
_get_arguments (struct arguments *args, int argc, char *argv[])
{
  struct arguments_definition def;
  struct arguments_option options[] = {
    { "Selection", 'a', "account", required_argument, "ACCOUNT",
      "only read entries for ACCOUNT" },
    { "Selection", 'f', "from_date", required_argument, "DATE",
      "only read entries with dates >= DATE" },
    { "Selection", 't', "to_date", required_argument, "DATE",
      "only read entries with dates <= DATE" },
    { "Selection", 'd', "date", required_argument, "DATE",
      "only read entries with dates matching DATE" },
    { "Transformation", 'i', "invert-amounts", no_argument, NULL,
      "invert te sign of all amounts" },
    { "Output control", 'A', "list-by-account", no_argument, NULL,
      "list the results grouped by account" },
    { "Output control", 'D', "list-by-date", no_argument, NULL,
      "list the results grouped by date" },
    { "Output control", 'T', "list-total", no_argument, NULL,
      "list the total amount" },
    { "Output control", 500, "list-details", no_argument, NULL,
      "list all the details (default)" },
    { "Miscellaneous", 'V', "version", no_argument, NULL,
      "print version information and exit" },
    { "Miscellaneous", 'h', "help", no_argument, NULL,
      "print this help and exit" },
    { NULL, 0, NULL, 0, NULL, NULL }
  };

  memset (&def, 0, sizeof (struct arguments_definition));

  def.print_usage_header = &_print_usage_header;
  def.process_option = &_process_option;
  def.process_non_options = &_process_non_options;
  def.options = options;

  memset (args, 0, sizeof (struct arguments));

  def.user_data = args;

  if (get_arguments (&def, argc, argv)) {
    if (!(args->list_details)
	&& !(args->list_by_account)
	&& !(args->list_by_date)
	&& !(args->list_total))
      args->list_details = true;

    return true;
  }

  return false;
}

static
void
add_entry (const char *account, const char *date, 
	   const char *desc, float amount)
{
  if (entries_used + 1 > entries_allocated) {
    struct entry *new_entries;
    size_t new_allocated = entries_allocated + 10;

    new_entries = (struct entry *) realloc (entries, new_allocated 
					    * sizeof (struct entry));
    if (NULL != new_entries) {
      entries = new_entries;
      entries_allocated = new_allocated;
    }
  }

  if (entries_used + 1 <= entries_allocated) {
    size_t b = entries_used;
    
    /* TODO: use binary search instead of iterative compare */
    for (size_t i = 0;i < entries_used;++i) {
      int rel;
      
      rel = strcmp (account, entries[i].account); 
      if (0 == rel)
	rel = strcmp (date, entries[i].date);
      if (rel < 0) {
	b = i;
	break;
      }
    }

    if (b < entries_used)
      memmove (entries + (b + 1), entries + b, 
	       (entries_used - b) * sizeof (struct entry));
    
    strncpy (entries[b].account, account, 4);
    strncpy (entries[b].date, date, 9);
    strncpy (entries[b].desc, desc, 80);
    entries[b].amount = amount;
    ++entries_used;
  }
}

/* This function reads a complete line and stores it into buffer.
 * The buffer will never be filled with more then size - 1 read characters.
 * If no error occurs the buffer will always be terminated and the return value
 * will contain the number of characters (not including the new line, if any)
 * read from the file (positive number).
 * If there is an error (return value is negative) the contents of the buffer
 * are unspecified.
 */
static
int
read_line (FILE *stream, char *buffer, size_t size)
{
  int rv;
  size_t cur = 0;
  bool add = size > 0;

  while ((rv = fgetc (stream)) >= 0) {
    if ('\n' == rv) {
      if (add) {
	buffer[cur] = '\0';
	add = 0;
      }
      break;
    } else { 
      if (add) {
	buffer[cur++] = rv;
	if (cur == size) { /* buffer full */
	  buffer[cur - 1] = '\0';
	  add = 0;
	}
      } else
	++cur;
    }
  }

  if (add && (EOF == rv || rv >= 0))
    buffer[cur] = '\0';

  return (EOF == rv || rv >= 0) ? cur : rv;
}

static
void
load_entries (struct arguments *args)
{
  size_t from_date_len, to_date_len, account_len;
  char line[101];
  int line_number = 0;

  from_date_len = strlen (args->from_date);
  to_date_len = strlen (args->to_date);
  account_len = strlen (args->account);

  if (NULL != entries) {
    free (entries);
    entries = NULL;
  }
  entries_allocated = entries_used = 0;
  
  while (!feof (args->input)) {
    int rv = read_line (args->input, line, sizeof (line));

    ++line_number;

    if (rv >= sizeof (line))
      fprintf (stderr, 
	       "warning: the line at %d is longer (%d) than we"
	       " can handle (%lu) and has been truncated\n",
	       line_number, rv, sizeof (line) - 1);
    
    if (rv > 0 && '#' != line[0]) {
      char date[9], account[4];
      float amount;
      int desc_start;

      if (3 == sscanf (line, "%8s %3s %f %n", date, 
		       account, &amount, &desc_start)) {
	size_t len;
	int i;
      
	if (args->invert_amounts)
	  amount = -amount;

	if ((0 == account_len
	     || 0 == strcmp (args->account, account))
	    && (0 == from_date_len 
		|| strncmp (args->from_date, date, from_date_len) <= 0)
	    && (0 == to_date_len 
		|| strncmp (args->to_date, date, to_date_len) >= 0)) {
	  len = strlen (line + desc_start);
	  if (len > 0) {
	    int i;
	    
	    for (i = len - 1;i >= 0;--i)
	      if ('\r' == line[desc_start + i] 
		  || '\n' == line[desc_start + i] 
		  || isspace (line[desc_start + i]))
		line[desc_start + i] = '\0';
	      else
		break;
	  }
	  add_entry (account, date, line + desc_start, amount);
	}
      }
    }
  }
}

static
void
do_list_details ()
{
  char account[4];
  float total_amount;
    
  account[0] = '\0';
    
  for (size_t i = 0;i <= entries_used;++i) {
    if (i == entries_used || 0 != strcmp (account, entries[i].account)) {
      if ('\0' != account[0])
	printf ("total    %#7.2F\n\n", total_amount);
      
      if (i < entries_used) {
	strcpy (account, entries[i].account);
	printf ("%-3s\n", account);
	total_amount = 0.0;
      }
    }

    if (i < entries_used) {
      total_amount += entries[i].amount;
      printf ("%8s %#7.2F %-58s\n", entries[i].date, entries[i].amount,
	      entries[i].desc);
    }
  }
}

static
void
do_list_by_account ()
{
  char account[4];
  float total_amount;

  account[0] = '\0';

  for (size_t i = 0;i <= entries_used;++i) {
    if (i == entries_used || 0 != strcmp (account, entries[i].account)) {
      if ('\0' != account[0])
	printf ("%-3s      %#7.2F\n", account, total_amount);

      if (i < entries_used) {
	strcpy (account, entries[i].account);
	total_amount = 0.0;
      }
    }

    if (i < entries_used)
      total_amount += entries[i].amount;
  }
}

static
void
do_list_by_date ()
{
  if (entries_used > 0) {
    struct summary {
      char date[9];
      float amount;
    } *summary = NULL;

    summary = (struct summary *) malloc (entries_used 
					 * sizeof (struct summary));
    if (NULL != summary) {
      size_t used = 0;

      for (size_t i = 0;i < entries_used;++i) {
	if (0 == used || strcmp (entries[i].date, summary[used - 1].date) > 0) {
	  strncpy (summary[used].date, entries[i].date, 9);
	  summary[used].amount = entries[i].amount;
	  ++used;
	} else {
	  size_t j;

	  for (j = 0;j < used;++j) {
	    int rel;

	    rel = strcmp (entries[i].date, summary[j].date);
	    if (0 == rel) {
	      summary[j].amount += entries[i].amount;
	      break;
	    } else if (rel < 0) {
	      memmove (summary + (j + 1), summary + j, 
		       (used - j) * sizeof (struct summary));
	      strncpy (summary[j].date, entries[i].date, 9);
	      summary[j].amount = entries[i].amount;
	      ++used;
	      break;
	    }
	  }
	}
      }

      for (size_t i = 0;i < used;++i)
	printf ("%8s %#7.2F\n", summary[i].date, summary[i].amount);

      free (summary);
    }
  }
}

static
void
do_list_total ()
{
  float total_amount = 0.0;
  
  for (size_t i = 0;i < entries_used;++i)
    total_amount += entries[i].amount;
  printf ("total    %#7.2F\n", total_amount);
}

/*
static
void
print_usage (const char *command)
{
  printf ("Usage: %s [OPTION]... [FILE]\n"
	  "Read a list and retrieve some data out of it.\n"
	  "\n"
	  "Selection:\n"
	  "  -a, --account=ACCOUNT\tonly read entries for ACCOUNT\n"
	  "  -f, --from-date=DATE\tonly read entries with dates >= DATE\n"
	  "  -t, --to-date=DATE\tonly read entries with dates <= DATE\n"
	  "  -d, --date=DATE\tonly read entries with dates matching DATE\n"
	  "\n"
	  "Transformation:\n"
	  "  -i, --invert-amounts\tinvert the sign of all amounts\n"
	  "\n"
	  "Output control:\n"
	  "  -A, --list-by-account\tlist the results grouped by account\n"
	  "  -D, --list-by-date\tlist the results grouped by date\n"
	  "  -T, --list-total\tlist the total amount\n"
	  "      --list-details\tlist all the details (default)\n"
	  "\n"
	  "Miscellaneous:\n"
	  "  -V, --version\t\tprint version information and exit\n"
	  "      --help\t\tdisplay this help and exit\n"
	  "\n"
	  "With no FILE, or when FILE is -, read standard input.\n",
	  command);
}
*/

int
main (int argc, char *argv[])
{
  struct arguments args;

  entries_allocated = entries_used = 0;
  entries = NULL;

  if (_get_arguments (&args, argc, argv)) {
    load_entries (&args);

    if (args.list_details)
      do_list_details ();

    if (args.list_by_account)
      do_list_by_account ();
  
    if (args.list_by_date)
      do_list_by_date ();

    if (args.list_total)
      do_list_total ();
  }

  return 0;
}
