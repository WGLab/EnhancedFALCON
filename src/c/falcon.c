/*
 * =====================================================================================
 *
 *       Filename:  fastcon.c
 *
 *    Description:  
 *
 *        Version:  0.1
 *        Created:  07/20/2013 17:00:00
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Jason Chin, 
 *        Company:  
 *
 * =====================================================================================

 #################################################################################$$
 # Copyright (c) 2011-2014, Pacific Biosciences of California, Inc.
 #
 # All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted (subject to the limitations in the
 # disclaimer below) provided that the following conditions are met:
 #
 #  * Redistributions of source code must retain the above copyright
 #  notice, this list of conditions and the following disclaimer.
 #
 #  * Redistributions in binary form must reproduce the above
 #  copyright notice, this list of conditions and the following
 #  disclaimer in the documentation and/or other materials provided
 #  with the distribution.
 #
 #  * Neither the name of Pacific Biosciences nor the names of its
 #  contributors may be used to endorse or promote products derived
 #  from this software without specific prior written permission.
 #
 # NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 # GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY PACIFIC
 # BIOSCIENCES AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 # WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 # OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 # DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 # SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 # LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 # USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 # ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 # OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 # OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 # SUCH DAMAGE.
 #################################################################################$$
 */

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "common.h"

typedef struct {
    seq_coor_t t_pos;
    unsigned int delta;
    char q_base;
    unsigned int q_id;
} align_tag_t;

typedef struct {
    seq_coor_t len;
    align_tag_t * align_tags;
} align_tags_t;


typedef struct {
    seq_coor_t len;
    char * name;
    char * seq;

} consensusn_seq_t;


align_tags_t * get_align_tags( char * aln_q_seq, 
                               char * aln_t_seq, 
                               seq_coor_t aln_seq_len,
                               aln_range * range,
                               unsigned long q_id,
                               unsigned long local_match_count_window,
                               unsigned long local_match_count_threshold,
                               seq_coor_t t_offset) {

#define LONGEST_INDEL_ALLOWED 6 

    char q_base;
    char t_base;
    align_tags_t * tags;
    seq_coor_t i, j, jj, k;
    seq_coor_t match_count;

    tags = calloc( 1, sizeof(align_tags_t) );
    tags->len = aln_seq_len; 
    tags->align_tags = calloc( aln_seq_len + 1, sizeof(align_tag_t) );
    i = range->s1 - 1;
    j = range->s2 - 1;
    match_count = 0;
    jj = 0;
    for (k = 0; k< local_match_count_window && k < aln_seq_len; k++) {
        if (aln_q_seq[k]  == aln_t_seq[k] ) {
            match_count ++;
        }
    }
    for (k = 0; k < aln_seq_len; k++) {
        if (aln_q_seq[k] != '-') {
            i ++;
            jj ++;
        } 
        if (aln_t_seq[k] != '-') {
            j ++;
            jj = 0;
        }
       
        if (local_match_count_threshold > 0) {
            if (k < aln_seq_len - local_match_count_window && aln_q_seq[k + local_match_count_window]  == aln_t_seq[k + local_match_count_window] ) {
                match_count ++;
            }

            if (k > local_match_count_window && aln_q_seq[k - local_match_count_window] == aln_t_seq[k - local_match_count_window] ) {
                match_count --;
            }

            if (match_count < 0) {
                match_count = 0;
            }
        }
       
        if ( j + t_offset >= 0) {
            (tags->align_tags[k]).t_pos = j + t_offset;
            (tags->align_tags[k]).delta = jj;
            if (local_match_count_threshold > 0 && jj == 0 && match_count < local_match_count_threshold) {
                (tags->align_tags[k]).q_base = '*';
            } else {
                (tags->align_tags[k]).q_base = aln_q_seq[k];
            }
            (tags->align_tags[k]).q_id = q_id;
        }
        //if (jj > LONGEST_INDEL_ALLOWED) {
        //   break;
        //}
    }
    // sentinal at the end
    //k = aln_seq_len;
    tags->len = k; 
    (tags->align_tags[k]).t_pos = -1;
    (tags->align_tags[k]).delta = -1;
    (tags->align_tags[k]).q_base = ' ';
    (tags->align_tags[k]).q_id = UINT_MAX;
    return tags;
}

void free_align_tags( align_tags_t * tags) {
    free( tags->align_tags );
    free( tags );
}


int compare_tags(const void * a, const void * b)
{
    const align_tag_t * arg1 = a;
    const align_tag_t * arg2 = b;
    if (arg1->delta - arg2->delta == 0) {
        return  arg1->q_base - arg2->q_base;
    } else {
        return arg1->delta - arg2->delta;
    }
}

consensus_data * get_cns_from_align_tags( align_tags_t ** tag_seqs, unsigned long n_tag_seqs, unsigned t_len, unsigned min_cov ) {

    seq_coor_t i, j, t_pos, tmp_pos;
    unsigned int * coverage;
    unsigned int * local_nbase;
    unsigned int * aux_index;

    unsigned int cur_delta;
    unsigned int counter[5] = {0, 0, 0, 0, 0};
    unsigned int k;
    unsigned int max_count;
    unsigned int max_count_index;
    seq_coor_t consensus_index;
    seq_coor_t c_start, c_end, max_start;
    unsigned int cov_score, max_cov_score;
    consensus_data * consensus;
    //char * consensus;



    align_tag_t ** tag_seq_index;

    coverage = calloc( t_len, sizeof(unsigned int) );
    local_nbase = calloc( t_len, sizeof(unsigned int) );
    aux_index = calloc( t_len, sizeof(unsigned int) );
    tag_seq_index = calloc( t_len, sizeof(align_tag_t *) );

    for (i = 0; i < n_tag_seqs; i++) {
        for (j = 0; j < tag_seqs[i]->len; j++) {
            if (tag_seqs[i]->align_tags[j].delta == 0 && tag_seqs[i]->align_tags[j].q_base != '*') {
                t_pos = tag_seqs[i]->align_tags[j].t_pos;
                coverage[ t_pos ] ++;
            }
            local_nbase[ tag_seqs[i]->align_tags[j].t_pos ] ++;
        }
    }


    for (i = 0; i < t_len; i++) {
        tag_seq_index[i] = calloc( local_nbase[i] + 1, sizeof(align_tag_t) );
    }

    for (i = 0; i < n_tag_seqs; i++) {
        for (j = 0; j < tag_seqs[i]->len; j++) {
            t_pos = tag_seqs[i]->align_tags[j].t_pos;
            tag_seq_index[ t_pos ][ aux_index[ t_pos ] ] = tag_seqs[i]->align_tags[j];
            aux_index[ t_pos ] ++;
        }
    }


    consensus_index = 0;

    
    consensus = calloc( 1, sizeof(consensus_data) );
    consensus->sequence = calloc( t_len * 2 + 1, sizeof(char) );
    consensus->eff_cov = calloc( t_len * 2 + 1, sizeof(unsigned int) );

    for (i = 0; i < t_len; i++) {
        qsort(tag_seq_index[i], local_nbase[i], sizeof(align_tag_t), compare_tags);
        cur_delta = 0;
        for (j = 0; j <= local_nbase[i]; j++) {
            max_count = 0;
            max_count_index = 0;
            if (j == local_nbase[i] || tag_seq_index[i][j].delta != cur_delta) {
                for (k = 0; k < 5; k ++) {
                    if (counter[k] > max_count) {
                        max_count = counter[k];
                        max_count_index = k;
                    }
                    //reset counter
                    counter[k] = 0;
                    cur_delta = tag_seq_index[i][j].delta;
                }
                if (max_count > coverage[i] * 0.5) { 
                    switch (max_count_index) {
                        case 0:
                            if (coverage[i] < min_cov + 1) {
                                consensus->sequence[consensus_index] = 'a';
                            } else {
                                consensus->sequence[consensus_index] = 'A';
                            }
                            consensus->eff_cov[consensus_index] = coverage[i] ;
                            consensus_index ++;
                            break;
                        case 1:
                            if (coverage[i] < min_cov + 1) {
                                consensus->sequence[consensus_index] = 'c';
                            } else {
                                consensus->sequence[consensus_index] = 'C';
                            }
                            consensus->eff_cov[consensus_index] = coverage[i] ;
                            consensus_index ++;
                            break;
                        case 2:
                            if (coverage[i] < min_cov + 1) {
                                consensus->sequence[consensus_index] = 'g';
                            } else {
                                consensus->sequence[consensus_index] = 'G';
                            }
                            consensus->eff_cov[consensus_index] = coverage[i] ;
                            consensus_index ++;
                            break;
                        case 3:
                            if (coverage[i] < min_cov + 1) {
                                consensus->sequence[consensus_index] = 't';
                            } else {
                                consensus->sequence[consensus_index] = 'T';
                            }
                            consensus->eff_cov[consensus_index] = coverage[i] ;
                            consensus_index ++;
                            break;
                        default:
                            break;
                    }
                    //printf("c:%c\n", consensus[consensus_index-1]);
                }

            } 

            if (j == local_nbase[i]) break;

            switch (tag_seq_index[i][j].q_base) {
                case 'A':
                    counter[0] ++;
                    break;
                case 'C':
                    counter[1] ++;
                    break;
                case 'G':
                    counter[2] ++;
                    break;
                case 'T':
                    counter[3] ++;
                    break;
                case '-':
                    counter[4] ++;
                    break;
                default:
                    break;
            }
            /*
            printf("%ld %ld %ld %u %c %u\n", i, j, tag_seq_index[i][j].t_pos,
                                                   tag_seq_index[i][j].delta,
                                                   tag_seq_index[i][j].q_base,
                                                   tag_seq_index[i][j].q_id);
            */
        }
    }
   
    //printf("%s\n", consensus);

    for (i = 0; i < t_len; i++) {
        free(tag_seq_index[i]);
    }
    free(tag_seq_index);
    free(aux_index);
    free(coverage);
    free(local_nbase);
    return consensus;
}

//const unsigned int K = 8;

consensus_data * generate_consensus( char ** input_seq, 
                           unsigned int n_seq, 
                           unsigned min_cov, 
                           unsigned K,
                           unsigned long local_match_count_window,
                           unsigned long local_match_count_threshold,
                           double min_idt) {

    unsigned int i, j, k;
    unsigned int seq_count;
    unsigned int aligned_seq_count;
    kmer_lookup * lk_ptr;
    seq_array sa_ptr;
    seq_addr_array sda_ptr;
    kmer_match * kmer_match_ptr;
    aln_range * arange_;
    aln_range * arange;
    alignment * aln;
    align_tags_t * tags;
    align_tags_t ** tags_list;
    //char * consensus;
    consensus_data * consensus;
    double max_diff;
    max_diff = 1.0 - min_idt;

    seq_count = n_seq;
    //for (j=0; j < seq_count; j++) {
    //    printf("seq_len: %u %u\n", j, strlen(input_seq[j]));
    //};
    fflush(stdout);

    tags_list = calloc( seq_count, sizeof(align_tags_t *) );
    lk_ptr = allocate_kmer_lookup( 1 << (K * 2) );
    sa_ptr = allocate_seq( (seq_coor_t) strlen( input_seq[0]) );
    sda_ptr = allocate_seq_addr( (seq_coor_t) strlen( input_seq[0]) );
    add_sequence( 0, K, input_seq[0], strlen(input_seq[0]), sda_ptr, sa_ptr, lk_ptr);
    //mask_k_mer(1 << (K * 2), lk_ptr, 16);

    aligned_seq_count = 0;
    for (j=1; j < seq_count; j++) {

        //printf("seq_len: %ld %u\n", j, strlen(input_seq[j]));

        kmer_match_ptr = find_kmer_pos_for_seq(input_seq[j], strlen(input_seq[j]), K, sda_ptr, lk_ptr);
#define INDEL_ALLOWENCE_0 6

        arange = find_best_aln_range(kmer_match_ptr, K, K * INDEL_ALLOWENCE_0, 5);  // narrow band to avoid aligning through big indels

        //printf("1:%ld %ld %ld %ld\n", arange_->s1, arange_->e1, arange_->s2, arange_->e2);

        //arange = find_best_aln_range2(kmer_match_ptr, K, K * INDEL_ALLOWENCE_0, 5);  // narrow band to avoid aligning through big indels

        //printf("2:%ld %ld %ld %ld\n\n", arange->s1, arange->e1, arange->s2, arange->e2);
        
#define INDEL_ALLOWENCE_1 400
        if (arange->e1 - arange->s1 < 100 || arange->e2 - arange->s2 < 100 ||
            abs( (arange->e1 - arange->s1 ) - (arange->e2 - arange->s2) ) > INDEL_ALLOWENCE_1) {
            free_kmer_match( kmer_match_ptr);
            free_aln_range(arange);
            continue;
        }
        //printf("%ld %s\n", strlen(input_seq[j]), input_seq[j]);
        //printf("%ld %s\n\n", strlen(input_seq[0]), input_seq[0]);
        
        
#define INDEL_ALLOWENCE_2 150

        aln = align(input_seq[j]+arange->s1, arange->e1 - arange->s1 ,
                    input_seq[0]+arange->s2, arange->e2 - arange->s2 , 
                    INDEL_ALLOWENCE_2, 1);
        if (aln->aln_str_size > 500 && ((double) aln->dist / (double) aln->aln_str_size) < max_diff) {
            tags_list[aligned_seq_count] = get_align_tags( aln->q_aln_str, 
                                                           aln->t_aln_str, 
                                                           aln->aln_str_size, 
                                                           arange, j, 
                                                           local_match_count_window,
                                                           local_match_count_threshold,
                                                           0); 
            aligned_seq_count ++;
        }
        /***
        for (k = 0; k < tags_list[j]->len; k++) {
            printf("%ld %d %c\n", tags_list[j]->align_tags[k].t_pos,
                                   tags_list[j]->align_tags[k].delta,
                                   tags_list[j]->align_tags[k].q_base);
        }
        ***/
        free_aln_range(arange);
        free_alignment(aln);
        free_kmer_match( kmer_match_ptr);
    }

    consensus = get_cns_from_align_tags( tags_list, aligned_seq_count, strlen(input_seq[0]), min_cov );
    //free(consensus);
    free_seq_addr_array(sda_ptr);
    free_seq_array(sa_ptr);
    free_kmer_lookup(lk_ptr);
    for (j=0; j < aligned_seq_count; j++) {
        free_align_tags(tags_list[j]);
    }
    free(tags_list);
    return consensus;
}

consensus_data * generate_utg_consensus( char ** input_seq, 
                           seq_coor_t *offset,
                           unsigned int n_seq, 
                           unsigned min_cov, 
                           unsigned K,
                           double min_idt) {

    unsigned int i, j, k;
    unsigned int seq_count;
    unsigned int aligned_seq_count;
    aln_range * arange;
    alignment * aln;
    align_tags_t * tags;
    align_tags_t ** tags_list;
    //char * consensus;
    consensus_data * consensus;
    double max_diff;
    seq_coor_t utg_len;
    seq_coor_t r_len;
    max_diff = 1.0 - min_idt;
    

    seq_count = n_seq;
    /***
    for (j=0; j < seq_count; j++) {
        printf("seq_len: %u %u\n", j, strlen(input_seq[j]));
    };
    fflush(stdout);
    ***/
    tags_list = calloc( seq_count+1, sizeof(align_tags_t *) );
    utg_len =  strlen(input_seq[0]);
    aligned_seq_count = 0;
    arange = calloc( 1, sizeof(aln_range) );

    arange->s1 = 0;
    arange->e1 = strlen(input_seq[0]);
    arange->s2 = 0;
    arange->e2 = strlen(input_seq[0]); 
    tags_list[aligned_seq_count] = get_align_tags( input_seq[0], input_seq[0], 
                                                   strlen(input_seq[0]), arange, 0, 
                                                   12, 0, 0); 
    aligned_seq_count += 1;
    for (j=1; j < seq_count; j++) {
        arange->s1 = 0;
        arange->e1 = strlen(input_seq[j])-1;
        arange->s2 = 0;
        arange->e2 = strlen(input_seq[j])-1; 

        r_len = strlen(input_seq[j]);
        //printf("seq_len: %u %u\n", j, r_len);
        if ( offset[j] < 0) {
            if ((r_len + offset[j]) < 128) {
                continue;
            }
            if ( r_len + offset[j] < utg_len ) {

                //printf("1: %ld %u %u\n", offset[j], r_len, utg_len);
                aln = align(input_seq[j] - offset[j], r_len + offset[j] ,
                            input_seq[0], r_len + offset[j] , 
                            500, 1);
            } else {
                //printf("2: %ld %u %u\n", offset[j], r_len, utg_len);
                aln = align(input_seq[j] - offset[j], utg_len ,
                            input_seq[0], utg_len , 
                            500, 1);
            }
            offset[j] = 0;

        } else {
            if ( offset[j] > utg_len - 128) {
                continue;
            }
            if ( offset[j] + r_len > utg_len ) {
                //printf("3: %ld %u %u\n", offset[j], r_len, utg_len);
                aln = align(input_seq[j], utg_len - offset[j] ,
                            input_seq[0]+offset[j], utg_len - offset[j], 
                            500, 1);
            } else {
                //printf("4: %ld %u %u\n", offset[j], r_len, utg_len);
                aln = align(input_seq[j], r_len ,
                            input_seq[0]+offset[j], r_len , 
                            500, 1);
            }
        }
        if (aln->aln_str_size > 500 && ((double) aln->dist / (double) aln->aln_str_size) < max_diff) {
            tags_list[aligned_seq_count] = get_align_tags( aln->q_aln_str, aln->t_aln_str, 
                                                           aln->aln_str_size, arange, j, 
                                                           12, 0, offset[j]); 
            aligned_seq_count ++;
        }
        free_alignment(aln);
    }
    free_aln_range(arange);
    consensus = get_cns_from_align_tags( tags_list, aligned_seq_count, utg_len, 0 );
    //free(consensus);
    for (j=0; j < aligned_seq_count; j++) {
        free_align_tags(tags_list[j]);
    }
    free(tags_list);
    return consensus;
}


void free_consensus_data( consensus_data * consensus ){
    free(consensus->sequence);
    free(consensus->eff_cov);
    free(consensus);
}

/***
void main() {
    unsigned int j;
    char small_buffer[1024];
    char big_buffer[65536];
    char ** input_seq;
    char ** seq_id;
    int seq_count;
    char * consensus;

    input_seq = calloc( 501, sizeof(char *));
    seq_id = calloc( 501, sizeof(char *));
    
    while(1) {
        seq_count = 0;
        while (1) {

            scanf("%s", small_buffer);
            seq_id[seq_count] = calloc( strlen(small_buffer) + 1, sizeof(char));
            strcpy(seq_id[seq_count], small_buffer);

            scanf("%s", big_buffer);
            input_seq[seq_count] = calloc( strlen(big_buffer) + 1 , sizeof(char));
            strcpy(input_seq[seq_count], big_buffer);

            if (strcmp(seq_id[seq_count], "+") == 0) {
                break;
            }
            if (strcmp(seq_id[seq_count], "-") == 0) {
                break;
            }
            //printf("%s\n", seq_id[seq_count]);
            seq_count += 1;
            if (seq_count > 500) break;
        }
        //printf("sc: %d\n", seq_count);
        if (seq_count < 10 && strcmp(seq_id[seq_count], "-") != 0 ) continue;
        if (seq_count < 10 && strcmp(seq_id[seq_count], "-") == 0 ) break;

            consensus = generate_consensus(input_seq, seq_count, 8, 8);
        if (strlen(consensus) > 500) {
            printf(">%s\n%s\n", seq_id[0], consensus);
        }
        fflush(stdout);
        free(consensus);
        for (j=0; j < seq_count; j++) {
            free(seq_id[j]);
            free(input_seq[j]);
        };

    }
    for (j=0; j < seq_count; j++) {
        free(seq_id[j]);
        free(input_seq[j]);
    };
    free(seq_id);
    free(input_seq);
}
***/
