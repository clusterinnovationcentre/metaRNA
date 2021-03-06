/**
 * Adapted from miRanda.
 *
 * Refactored by: Prashant Sinha (prashant@ducic.ac.in) on 24 Feb 2016
 *
 * Original Authors: Anton Enright, Bino John, Chris Sander and Debora Marks
 * Copyright (C) (2003) Memorial Sloan-Kettering Cancer Center, New York
 * Distributed under the GNU Public License (GPL)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "miranda.h"

extern int strict;
extern double score_threshold;
extern double energy_threshold;
extern int length_5p_for_weighting;
extern int length_3p_for_weighting;
extern int key_value_pairs;
extern int alignment_len_threshold;

/* summary of best hits for reporting - for convenient argument passing */
typedef struct HitSummaryT
{
  double scan_score;
  int no_hits;
  double max_hit;
  double max_score;
  double total_score;
  ExpString *position_list;
} HitSummary;

const int INITIAL_STRING_SIZE = 64;

double do_alignment(int** best, int*** track, int** a_nt_nt, int** b_gap_nt,
      int** c_nt_gap, int** nt_nt_score, char* query_sequence,
      char* reference_sequence, score_struct* scores, int query_length,
      int reference_length, int verbose, HitSummary* hit_summary,
      hit_struct* hit, ExpString* outjson) {

  int i = 0;
  int j = 0;
  int utr_offset3p;
  int utr_offset5p;

  double energy = 0;
  double scan_score = 0;

  int good_call = 0;
  int diff = 0;

  char strict_query_construct[200];
  char strict_alignment_construct[200];

  int non_gap_count = 0;
  int perfect_match_count = 0;
  int gap_count = 0;
  int mypos = 0;

  double hit_score;

  int tmp_integer;
  int* good_ones_starts_j, *good_ones_ends_j, good_ones_count;
  int scores_length = 0;

  char delim[3] = "";

  good_ones_count = -1;
  good_ones_starts_j = (int*)calloc(reference_length, sizeof(int));
  good_ones_ends_j = (int*)calloc(reference_length, sizeof(int));

  hit_summary->no_hits = 0;
  hit_summary->max_hit = 0;
  hit_summary->max_score = 0;
  hit_summary->scan_score = 0;
  hit_summary->total_score = 0;

  get_nt_nt_seq_scores(nt_nt_score, query_sequence, reference_sequence,
      query_length, reference_length);

  build_matrix(best, track, a_nt_nt, b_gap_nt, c_nt_gap, nt_nt_score,
      query_sequence, reference_sequence, query_length, reference_length,
      scores, &scores_length);

  for (i = 0; i <= scores_length; i++) {
    utr_offset3p = 0;
    utr_offset5p = 0;
    good_call = 1;
    clear_hit(hit, query_length, reference_length);
    hit_score = scores[i].score;

    if (hit_score >= score_threshold) {
      traceback(best, track, query_sequence, reference_sequence,
          scores[i].query_trace_end, scores[i].reference_trace_end,
          hit, hit_score);

      good_call = testfor_overlap(good_ones_starts_j, good_ones_ends_j,
          &good_ones_count, hit->ref_start, hit->ref_end);

      if (good_call == 1) {
        good_ones_starts_j[good_ones_count] = hit->ref_start;
        good_ones_ends_j[good_ones_count] = hit->ref_end;
      }

      /**
       * miRNA alignment, convert un-aligned nt to lowercase, aligned nt to uppercase
       * hit->rest[0, 1, 2] = The 5' unaligned regions in the query
       * hit->rest[3, 4, 5] = The 3' unaligned regions in the query
       */

      if (hit->query_start >= 1) {
        for (j = 0; j <= hit->query_start - 1; j++) {
          diff = hit->query_start - j;
          hit->rest[0][j] = tolower(query_sequence[j]);
          tmp_integer = hit->ref_start - diff;

          if (tmp_integer >= 0) {
            hit->rest[1][j] = tolower(reference_sequence[tmp_integer]);
            utr_offset3p++;
          } else {
            hit->rest[1][j] = '-';
          }
          hit->rest[2][j] = ' ';
        }
      }

      if (hit->query_end < query_length) {
        for (j = hit->query_end; j < query_length; j++) {
          diff = j - hit->query_end;
          hit->rest[3][j - hit->query_end] = tolower(query_sequence[j]);
          tmp_integer = hit->ref_end + diff;

          if (tmp_integer < reference_length) {
            hit->rest[4][j - hit->query_end] =
                tolower(reference_sequence[tmp_integer]);
            utr_offset5p++;
          } else {
            hit->rest[4][j - hit->query_end] = '-';
          }

          hit->rest[5][j - hit->query_end] = ' ';
        }
      }

      string_toupper(hit->alignment[0]);
      string_toupper(hit->alignment[2]);

      /* Adjusting for offset due to local alignment in next two lines*/
      hit->ref_end += (utr_offset5p - 1);
      hit->ref_start -= utr_offset3p;
      mypos = 0;
      non_gap_count = perfect_match_count = gap_count = 0;

      /* looks for strict seed matches*/
      if (strict) {
        sprintf(strict_query_construct, "%s%s%s", hit->rest[3],
            hit->alignment[0], hit->rest[0]);
        sprintf(strict_alignment_construct, "%s%s%s", hit->rest[5],
            hit->alignment[1], hit->rest[2]);

        /*traverse the alignment*/
        for (j = 0; j < (int)strlen(strict_query_construct); j++) {
          if (strict_query_construct[j] != '-') {
            /*if no gaps in the miRNA alignment*/
            mypos++;
          }

          if ((mypos >= 2) && (mypos <= 8)) {
            if (strict_alignment_construct[j] != ' ') {
              /*if no gaps in the alignment i.e. if `|` or `:`*/
              non_gap_count++;
            }
            if (strict_alignment_construct[j] == '|') {
              perfect_match_count++;
            }
            if (strict_query_construct[j] == '-') {
              gap_count++;
            }
          }

          if (mypos == 8) {
            break;
          }
        }
        /*fail if less than 7 perfect matches or any gaps*/
        if (non_gap_count < 7 || perfect_match_count < 7 || gap_count > 0) {
          good_call = 0;
        }
      }

      energy = get_energy(hit);

      int alignment_length = strlen(hit->alignment[0]);

      if (alignment_length >= alignment_len_threshold) {
        /* good_call, a good alignment that passes score, energy, etc. */
        if (good_call) {
          scan_score += (energy * -1);
          hit_summary->no_hits++;
          append_string_ExpString(hit_summary->position_list, delim);
          append_int_ExpString(hit_summary->position_list, hit->ref_start + 1);

          if (energy < hit_summary->max_hit) {
            hit_summary->max_hit = energy;
          }
          hit_summary->total_score += hit->score;
          if (hit->score > hit_summary->max_score) {
            hit_summary->max_score = hit->score;
          }

          append_string_ExpString(outjson, delim);
          printhit(query_length, hit, energy, outjson);
          strcpy(delim, ", ");
        }
      }
    }
    scores[i].score = 0;
    scores[i].path = 0;
    scores[i].query_trace_end = 0;
    scores[i].reference_trace_end = 0;
  }
  free(good_ones_ends_j);
  free(good_ones_starts_j);
  return scan_score;
}

/* Load Sequences and Set-up the Alignment Run*/
void find_targets(char* gene_seq, char* mirna_seq, ExpString* outjson) {
  int** best;         // Best score of all three states (nt-nt, nt-gap, gap-nt)
  int*** track;       // Traceback Matrix
  int** a_nt_nt;      // Best score for state nt-nt
  int** b_gap_nt;     // Best score for state gap-nt
  int** c_nt_gap;     // Best score for state nt-gap
  int** nt_nt_score;  // nt nt match matrix for easy lookup

  int gene_len  = strlen(gene_seq);
  int mirna_len = strlen(mirna_seq);

  int i = 0;

  hit_struct hit;           // Struct to store hit information
  score_struct* scores;     // Score struct for non-optimal path detection
  HitSummary hit_summary;   // Summary of best hits for reporting

  double end_score = 0.0;

  hit_summary.position_list = 0;

  create_ExpString(&(hit_summary.position_list));
  append_string_ExpString(hit_summary.position_list, "[");

  /* Keep track of the number of sequences scanned so far*/
  length_3p_for_weighting = mirna_len - length_5p_for_weighting;

  /* Initialize the hit / alignment constructs for this sequence*/
  hit.alignment[0] = calloc(mirna_len + gene_len, sizeof(char));
  hit.alignment[1] = calloc(mirna_len + gene_len, sizeof(char));
  hit.alignment[2] = calloc(mirna_len + gene_len, sizeof(char));
  hit.rest[0] = calloc(mirna_len + 10, sizeof(char));
  hit.rest[1] = calloc(mirna_len + 10, sizeof(char));
  hit.rest[2] = calloc(mirna_len + 10, sizeof(char));
  hit.rest[3] = calloc(mirna_len + 10, sizeof(char));
  hit.rest[4] = calloc(mirna_len + 10, sizeof(char));
  hit.rest[5] = calloc(mirna_len + 10, sizeof(char));

  /* Structure for sub-optimal score list*/
  scores = (score_struct*)calloc(mirna_len * gene_len, sizeof(score_struct));

  /* Initialize the three alignment matrices*/
  best = calloc((mirna_len + 1), sizeof(int*));
  track = (int***)calloc(4, sizeof(int**));
  a_nt_nt = calloc((mirna_len + 1), sizeof(int*));
  b_gap_nt = calloc((mirna_len + 1), sizeof(int*));
  c_nt_gap = calloc((mirna_len + 1), sizeof(int*));
  nt_nt_score = calloc((mirna_len + 1), sizeof(int*));

  /*Initialize 4-D call-back matrix*/
  for (i = 0; i < 4; i++) {
    track[i]= (int**)calloc((mirna_len + 1), sizeof(int*));
  }

  for (i = 0; i <= mirna_len; i++) {
    best[i] = calloc((gene_len + 1), sizeof(int));
    track[0][i] = (int*)calloc((gene_len + 1), sizeof(int));
    track[1][i] = (int*)calloc((gene_len + 1), sizeof(int));
    track[2][i] = (int*)calloc((gene_len + 1), sizeof(int));
    track[3][i] = (int*)calloc((gene_len + 1), sizeof(int));
    a_nt_nt[i] = calloc((gene_len + 1), sizeof(int));
    b_gap_nt[i] = calloc((gene_len + 1), sizeof(int));
    c_nt_gap[i] = calloc((gene_len + 1), sizeof(int));
    nt_nt_score[i] = calloc((gene_len + 1), sizeof(int));
    best[i][0] = a_nt_nt[i][0] = nt_nt_score[i][0] = 0;
    track[0][i][0] = track[1][i][0] = track[2][i][0] = track[3][i][0] = 0;
    b_gap_nt[i][0] = 0;
    c_nt_gap[i][0] = 0;
  }

  for (i = 0; i < gene_len + 1; i++) {
    best[0][i] = a_nt_nt[0][i] = nt_nt_score[0][i] = 0;
    b_gap_nt[0][i] = 0;
    c_nt_gap[0][i] = 0;
    track[0][0][i] = track[1][0][i] = track[2][0][i] = track[3][0][i] = 0;
  }

  append_string_ExpString(outjson, "{\"hits\":[");

  end_score = do_alignment(best, track, a_nt_nt, b_gap_nt, c_nt_gap,
      nt_nt_score, mirna_seq, gene_seq, scores, mirna_len, gene_len,
      1, &hit_summary, &hit, outjson);

  append_string_ExpString(hit_summary.position_list, "]");
  append_string_ExpString(outjson, "], ");

  int len_pos = length_ExpString(hit_summary.position_list);
  char *temp;

  temp = (char *)malloc(len_pos + 256);

  if (end_score > 0.0) {
    sprintf(temp,
      "\"digest\": {\"total_score\": %2.2f, \"total_energy\": %2.2f, "
      "\"max_score\": %2.2f, \"max_energy\": %2.2f, "
      "\"mirna_len\": %d, \"gene_len\": %d, \"positions\": %s}",
        hit_summary.total_score,
        end_score, hit_summary.max_score, hit_summary.max_hit,
        mirna_len, gene_len,
        access_ExpString(hit_summary.position_list)
    );
  } else {
    sprintf(temp, "\"digest\": {\"error\": true}");
  }
  append_string_ExpString(outjson, temp);
  append_string_ExpString(outjson, "}");
  free(temp);

  for (i = mirna_len; i >= 0; i--) {
    free(nt_nt_score[i]);
    free(c_nt_gap[i]);
    free(b_gap_nt[i]);
    free(a_nt_nt[i]);
    free(track[3][i]);
    free(track[2][i]);
    free(track[1][i]);
    free(track[0][i]);
    free(best[i]);
  }
  for (i = 3; i >= 0; i--) {
    free(track[i]);
  }
  free(nt_nt_score);
  free(c_nt_gap);
  free(b_gap_nt);
  free(a_nt_nt);
  free(track);
  free(best);
  free(scores);
  free(hit.rest[5]);
  free(hit.rest[4]);
  free(hit.rest[3]);
  free(hit.rest[2]);
  free(hit.rest[1]);
  free(hit.rest[0]);
  free(hit.alignment[2]);
  free(hit.alignment[1]);
  free(hit.alignment[0]);

  destroy_ExpString(&(hit_summary.position_list));
}
