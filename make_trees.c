#define _MAIN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gbpLib.h>
#include <gbpMath.h>
#include <gbpHalos.h>
#include <gbpTrees.h>

int main(int argc, char *argv[]){
  char        filename_halo_root_in[256];
  char        filename_cat_root_in[256];
  char        filename_root_out[256];
  char        filename_output_dir_horizontal_trees[256];
  char        filename_output_dir[256];
  char        filename_output_dir_horizontal[256];
  char        filename_root_matches[256];
  char        filename_snap_list_in[256];
  char        filename_snap_list_out[256];
  char        filename_output_file_root[256];
  int         i_read_start;
  int         i_read_stop;
  int         i_read_step;
  int         n_search;
  int         n_files_groups;
  int         n_files_subgroups;
  int         n_k_match=2;
  int         flag_clean=FALSE;
  FILE       *fp_in;
  FILE       *fp_out;
  char       *line=NULL;
  size_t      line_length=0;
  int         n_snaps,i_read,i_next,i_write,n_keep;
  double     *a_list_in;
  double     *a_list_out;
  cosmo_info *cosmo;
  int         flag_fix_bridges;

  SID_init(&argc,&argv,NULL);

  // Initialize cosmology
  init_cosmo_std(&cosmo);

  // Fetch user inputs
  if(argc!=13)
    SID_trap_error("Incorrect syntax",ERROR_SYNTAX);
  strcpy(filename_halo_root_in,argv[1]);
  strcpy(filename_cat_root_in, argv[2]);
  strcpy(filename_root_matches,argv[3]);
  strcpy(filename_root_out,    argv[4]);
  strcpy(filename_snap_list_in,argv[5]);
  i_read_start     =atoi(argv[6]);
  i_read_stop      =atoi(argv[7]);
  i_read_step      =atoi(argv[8]);
  n_search         =atoi(argv[9]);
  n_files_groups   =atoi(argv[10]);
  n_files_subgroups=atoi(argv[11]);
  flag_fix_bridges =atoi(argv[12]);

  SID_log("Constructing merger trees for snapshots #%d->#%d (step=%d, n_search=%d)...",SID_LOG_OPEN|SID_LOG_TIMER,i_read_start,i_read_stop,i_read_step,n_search);

  // Read snapshot expansion factor list
  if(SID.I_am_Master){
    strcpy(filename_output_file_root,filename_root_out);
    strip_path(filename_output_file_root);
    mkdir(filename_root_out,02755);
    sprintf(filename_snap_list_out,"%s/a_list.txt",filename_root_out);
    SID_log("Reading snapshot list {%s}...",SID_LOG_OPEN,filename_snap_list_in);

    // Read the original list
    fp_in     =fopen(filename_snap_list_in, "r");
    n_snaps   =count_lines_data(fp_in);
    a_list_in =(double *)SID_malloc(sizeof(double)*n_snaps);
    a_list_out=(double *)SID_malloc(sizeof(double)*n_snaps);
    for(i_read=0;i_read<n_snaps;i_read++){
      grab_next_line_data(fp_in,&line,&line_length);
      grab_double(line,1,&(a_list_in[i_read]));
    }
    fclose(fp_in);

    // Select the snapshots we'll use
    for(i_read=i_read_stop,i_next=i_read_stop,n_keep=0;i_read>=i_read_start;i_read--){
      if(i_read==i_next){
        a_list_out[n_keep++]=a_list_in[i_read];
        i_next-=i_read_step;
      }
    }

    // Write them to a file
    fp_out=fopen(filename_snap_list_out,"w");
    for(i_write=n_keep-1;i_write>=0;i_write--)
      fprintf(fp_out,"%le\n",a_list_out[i_write]);
    fclose(fp_out);

    SID_free(SID_FARG a_list_in);
    SID_log("Done.",SID_LOG_CLOSE);
  }

  compute_trees_horizontal(filename_halo_root_in,
                           filename_cat_root_in,
                           filename_root_matches,
                           filename_root_out,
                           a_list_out,
                           &cosmo,
                           i_read_start,
                           i_read_stop,
                           i_read_step,
                           n_search,
                           flag_fix_bridges,
                           &flag_clean);
  flag_clean=FALSE;
  compute_trees_vertical(filename_root_out,
                         filename_cat_root_in,
                         i_read_start,
                         i_read_stop,
                         i_read_step,
                         n_files_groups,
                         n_files_subgroups,
                         &flag_clean);

  SID_log("Done.",SID_LOG_CLOSE);

  // Clean-up
  if(SID.I_am_Master)
     SID_free(SID_FARG a_list_out);
  ADaPS_free(SID_FARG cosmo);  

  SID_exit(ERROR_NONE);
}
