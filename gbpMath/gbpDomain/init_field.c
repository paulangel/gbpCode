// Initialize the field, k/real-space domains and FFTW plans
#include <stdio.h>
#include <stdlib.h>
#include <gbpLib.h>
#include <gbpDomain.h>

void init_field(int        n_d,
                int        *n,
                double     *L,
                field_info *FFT){
  int  i_d;
  int  i_i;
  int  n_x_local;
  int  i_x_start_local;
  int  n_y_transpose_local;
  int  i_y_start_transpose_local;
  int *n_x_rank;
  int  i_rank;
  int  j_rank;
  int  flag_active;
  int  n_active;
  int  min_size,max_size;
  int  total_local_size_int;

  // Make sure that fftw and gbpLib are compiled with the same precision
  if(sizeof(fftw_real)!=sizeof(REAL))
    SID_trap_error("Size of an fftw_real (%lld) is not the same the size of a REAL (%lld).  Fix installation!",
                   ERROR_LOGIC,
                   sizeof(fftw_real),
                   sizeof(REAL));

  SID_log("Initializing ",SID_LOG_OPEN);
  for(i_d=0;i_d<n_d;i_d++){
    if(i_d<(n_d-1))
      SID_log("%dx",SID_LOG_CONTINUE,n[i_d]);
    else
      SID_log("%d element %d-d FFT ",SID_LOG_CONTINUE,n[i_d],n_d);
  }
  SID_log("(%d byte precision)...",SID_LOG_CONTINUE,(int)sizeof(fftw_real));

  // Initialize FFT sizes
  FFT->n_d            =n_d;
  FFT->n              =(int    *)malloc(sizeof(int)*FFT->n_d);
  FFT->L              =(double *)malloc(sizeof(double)*FFT->n_d);
  FFT->n_k_local      =(int *)malloc(sizeof(int)*FFT->n_d);
  FFT->n_R_local      =(int *)malloc(sizeof(int)*FFT->n_d);
  FFT->i_R_start_local=(int *)malloc(sizeof(int)*FFT->n_d);
  FFT->i_k_start_local=(int *)malloc(sizeof(int)*FFT->n_d);
  FFT->i_R_stop_local =(int *)malloc(sizeof(int)*FFT->n_d);
  FFT->i_k_stop_local =(int *)malloc(sizeof(int)*FFT->n_d);
  for(i_d=0;i_d<FFT->n_d;i_d++){
    FFT->n[i_d]              =n[i_d];
    FFT->L[i_d]              =L[i_d];
    FFT->i_R_start_local[i_d]=0;
    FFT->i_k_start_local[i_d]=0;
    FFT->n_R_local[i_d]      =FFT->n[i_d];
    FFT->n_k_local[i_d]      =FFT->n[i_d];
  }
  FFT->n_k_local[FFT->n_d-1]=FFT->n[FFT->n_d-1]/2;

  // Initialize FFTW
  #ifdef USE_MPI
    FFT->plan =rfftwnd_mpi_create_plan(MPI_COMM_WORLD,
                                       FFT->n_d,FFT->n,
                                       FFTW_REAL_TO_COMPLEX,
                                       FFTW_ESTIMATE);
    FFT->iplan=rfftwnd_mpi_create_plan(MPI_COMM_WORLD,
                                       FFT->n_d,FFT->n,
                                       FFTW_COMPLEX_TO_REAL,
                                       FFTW_ESTIMATE);
    rfftwnd_mpi_local_sizes(FFT->plan,
			    &(n_x_local), 
			    &(i_x_start_local),
			    &(n_y_transpose_local),
			    &(i_y_start_transpose_local),
			    &total_local_size_int);
    FFT->total_local_size=(size_t)total_local_size_int;
    if(n_x_local==0)
      i_x_start_local=0;
    if(n_y_transpose_local==0)
      i_y_start_transpose_local=0;
    FFT->i_R_start_local[0]=i_x_start_local;
    FFT->n_R_local[0]      =n_x_local;
    if(FFT->n_d>1){
      FFT->i_k_start_local[1]=i_y_start_transpose_local;
      FFT->n_k_local[1]      =n_y_transpose_local;
    }
  #else
    for(i_d=0,FFT->total_local_size=1;i_d<FFT->n_d;i_d++){
      if(i_d<FFT->n_d-1)
        FFT->total_local_size*=FFT->n[i_d];
      else
        FFT->total_local_size*=2*(FFT->n[i_d]/2+1);
    }
    FFT->plan =rfftwnd_create_plan(FFT->n_d,FFT->n,FFTW_REAL_TO_COMPLEX,FFTW_ESTIMATE|FFTW_IN_PLACE);
    FFT->iplan=rfftwnd_create_plan(FFT->n_d,FFT->n,FFTW_COMPLEX_TO_REAL,FFTW_ESTIMATE|FFTW_IN_PLACE);
  #endif
  FFT->field_local =(fftw_real *)SID_malloc(sizeof(fftw_complex)*FFT->total_local_size); // real?

  // Upper limits of slab decomposition
  for(i_d=0;i_d<FFT->n_d;i_d++){
    FFT->i_R_stop_local[i_d]=FFT->i_R_start_local[i_d]+FFT->n_R_local[i_d]-1;
    FFT->i_k_stop_local[i_d]=FFT->i_k_start_local[i_d]+FFT->n_k_local[i_d]-1;
  }

  // FFTW padding sizes
  if(FFT->n_d>1){
    FFT->pad_size_R=2*(FFT->n_R_local[FFT->n_d-1]/2+1)-FFT->n_R_local[FFT->n_d-1];
    FFT->pad_size_k=1;
  }
  else{
    FFT->pad_size_R=0;
    FFT->pad_size_k=0;
  }

  // Number of elements (global and local) in the FFT
  for(i_d=0,FFT->n_field=1,FFT->n_field_R_local=1,FFT->n_field_k_local=1;i_d<FFT->n_d;i_d++){
    FFT->n_field        *=(size_t)FFT->n[i_d];
    FFT->n_field_R_local*=(size_t)FFT->n_R_local[i_d];
    FFT->n_field_k_local*=(size_t)FFT->n_k_local[i_d];
  }

  // A pointer for referencing the field as a complex array
  FFT->cfield_local=(fftw_complex *)FFT->field_local;
  clear_field(FFT);

  // Initialize the FFT's real-space grid
  FFT->R_field=(double **)malloc(sizeof(double *)*FFT->n_d);
  FFT->dR     =(double  *)malloc(sizeof(double *)*FFT->n_d);
  for(i_d=0;i_d<FFT->n_d;i_d++){
    FFT->R_field[i_d]=(double *)malloc(sizeof(double)*(FFT->n[i_d]+1));
    FFT->dR[i_d]     =FFT->L[i_d]/(double)(FFT->n[i_d]);
    for(i_i=0;i_i<FFT->n[i_d];i_i++)
      FFT->R_field[i_d][i_i]=FFT->L[i_d]*((double)i_i/(double)(FFT->n[i_d]));
    FFT->R_field[i_d][FFT->n[i_d]]=FFT->L[i_d];
  }

  // Initialize the FFT's k-space grid
  FFT->k_field  =(double **)malloc(sizeof(double *)*FFT->n_d);
  FFT->dk       =(double  *)malloc(sizeof(double *)*FFT->n_d);
  FFT->k_Nyquist=(double  *)malloc(sizeof(double *)*FFT->n_d);
  for(i_d=0;i_d<FFT->n_d;i_d++){
    FFT->k_field[i_d]  =(double *)malloc(sizeof(double)*FFT->n[i_d]);
    FFT->dk[i_d]       =TWO_PI/FFT->L[i_d];
    FFT->k_Nyquist[i_d]=TWO_PI*(double)(FFT->n[i_d])/FFT->L[i_d]/2.;
    for(i_i=0;i_i<FFT->n[i_d];i_i++){
      if(i_i>=FFT->n[i_d]/2)
        FFT->k_field[i_d][i_i]=TWO_PI*(double)(i_i-FFT->n[i_d])/FFT->L[i_d];
      else
        FFT->k_field[i_d][i_i]=TWO_PI*(double)(i_i)/FFT->L[i_d];
    }
  }

  // Flags
  FFT->flag_padded=FALSE;

  // Slab info
  FFT->slab.n_x_local      =FFT->n_R_local[0];
  FFT->slab.i_x_start_local=FFT->i_R_start_local[0];
  FFT->slab.i_x_stop_local =FFT->i_R_stop_local[0];
  FFT->slab.x_min_local    =FFT->R_field[0][FFT->i_R_start_local[0]];
  if(FFT->slab.n_x_local>0)
    FFT->slab.x_max_local  =FFT->R_field[0][FFT->i_R_stop_local[0]+1];
  else
    FFT->slab.x_max_local  =FFT->slab.x_min_local;
  FFT->slab.x_max          =FFT->R_field[0][FFT->n[0]];

#ifdef USE_MPI

  // All ranks are not necessarily assigned any slices, so
  //   we need to figure out what ranks are to the right and the left for
  //   buffer exchanges
  n_x_rank=(int *)malloc(sizeof(int)*SID.n_proc);
  n_x_rank[SID.My_rank]=FFT->slab.n_x_local;
  if(n_x_rank[SID.My_rank]>0)
    flag_active=TRUE;
  else
    flag_active=FALSE;
  MPI_Allreduce(&flag_active,          &n_active,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);
  MPI_Allreduce(&n_x_rank[SID.My_rank],&min_size,1,MPI_INT,MPI_MIN,MPI_COMM_WORLD);
  MPI_Allreduce(&n_x_rank[SID.My_rank],&max_size,1,MPI_INT,MPI_MAX,MPI_COMM_WORLD);
  for(i_rank=0;i_rank<SID.n_proc;i_rank++)
    MPI_Bcast(&(n_x_rank[i_rank]),1,MPI_INT,i_rank,MPI_COMM_WORLD);
  FFT->slab.rank_to_right=-1;
  for(i_rank=SID.My_rank+1;i_rank<SID.My_rank+SID.n_proc && FFT->slab.rank_to_right<0;i_rank++){
    j_rank=i_rank%SID.n_proc;
    if(n_x_rank[j_rank]>0) 
      FFT->slab.rank_to_right=j_rank;
  }
  if(FFT->slab.rank_to_right<0)
    FFT->slab.rank_to_right=SID.My_rank;
  FFT->slab.rank_to_left=-1;
  for(i_rank=SID.My_rank-1;i_rank>SID.My_rank-SID.n_proc && FFT->slab.rank_to_left<0;i_rank--){
    if(i_rank<0)
      j_rank=i_rank+SID.n_proc;
    else
      j_rank=i_rank;
    if(n_x_rank[j_rank]>0) FFT->slab.rank_to_left=j_rank;
  }  
  if(FFT->slab.rank_to_left<0)
    FFT->slab.rank_to_left=SID.My_rank;
  free(n_x_rank);
  SID_log("(%d cores unused, min/max slab size=%d/%d)...",SID_LOG_CONTINUE,SID.n_proc-n_active,min_size,max_size);
#else
  FFT->slab.rank_to_right=SID.My_rank;
  FFT->slab.rank_to_left =SID.My_rank;
  if(FFT->slab.n_x_local>0){
    flag_active=TRUE;
    n_active   =1;
    min_size   =FFT->slab.n_x_local;
    max_size   =FFT->slab.n_x_local;
  }
  else{
    flag_active=FALSE;
    n_active   =0;
    min_size   =0;
    max_size   =0;
  }
#endif

  SID_log("Done.",SID_LOG_CLOSE);
}
