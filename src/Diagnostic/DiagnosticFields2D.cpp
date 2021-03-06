
#include "DiagnosticFields2D.h"

#include <sstream>
#include <cmath> 
#include <limits>

#include "Params.h"
#include "Patch.h"
#include "Field2D.h"
#include "VectorPatch.h"
#include "Hilbert_functions.h"

using namespace std;

DiagnosticFields2D::DiagnosticFields2D( Params &params, SmileiMPI* smpi, Patch* patch, int ndiag )
    : DiagnosticFields( params, smpi, patch, ndiag )
{
    
    // Calculate the offset in the local grid
    patch_offset_in_grid.resize(2);
    patch_offset_in_grid[0] = params.oversize[0];
    patch_offset_in_grid[1] = params.oversize[1];
    
    // Calculate the patch size
    patch_size.resize(2);
    patch_size[0] = params.n_space[0] + 1;
    patch_size[1] = params.n_space[1] + 1;
    total_patch_size = patch_size[0] * patch_size[1];
    
    
    // define space in file
    hsize_t global_size[1];
    global_size[0] = tot_number_of_patches * total_patch_size;
    filespace_firstwrite = H5Screate_simple(1, global_size, NULL);
    memspace_firstwrite = H5Screate_simple(1, global_size, NULL ); // redefined later
    
    // Define a second subset of the grid, which is unrelated to the current 
    // composition of vecPatches. It is used for a second writing of the file
    // in order to fold the Hilbert curve. This new subset is necessarily
    // rectangular for efficient writing.
    hsize_t offset[1], block[1], count[1];
    int nproc = smpi->getSize(), iproc = smpi->getRank();
    int npatch = params.tot_number_of_patches;
    int npatch_local = 1<<int(log2( ((double)npatch)/nproc ));
    int first_proc_with_less_patches = (npatch-npatch_local*nproc)/npatch_local;
    int first_patch_of_this_proc;
    if( iproc < first_proc_with_less_patches ) {
        npatch_local *= 2;
        first_patch_of_this_proc = npatch_local*iproc;
    } else {
        first_patch_of_this_proc = npatch_local*(first_proc_with_less_patches+iproc);
    }
    // Define space in file for re-reading
    filespace_reread = H5Screate_simple(1, global_size, NULL);
    offset[0] = total_patch_size * first_patch_of_this_proc;
    block [0] = total_patch_size * npatch_local;
    count [0] = 1;
    H5Sselect_hyperslab(filespace_reread, H5S_SELECT_SET, offset, NULL, count, block);
    // Define space in memory for re-reading
    memspace_reread = H5Screate_simple(1, block, NULL);
    data_reread.resize( block[0] );
    // Define the list of patches for re-writing
    rewrite_npatch = (unsigned int)npatch_local;
    rewrite_patches_x.resize( rewrite_npatch );
    rewrite_patches_y.resize( rewrite_npatch );
    rewrite_xmin=numeric_limits<int>::max(); rewrite_ymin=numeric_limits<int>::max();
    unsigned int rewrite_xmax=0, rewrite_ymax=0, x, y;
    for( unsigned int h=0; h<rewrite_npatch; h++) {
        generalhilbertindexinv(params.mi[0],  params.mi[1], &x, &y, first_patch_of_this_proc+h);
        if(x<rewrite_xmin) rewrite_xmin=x;
        if(x>rewrite_xmax) rewrite_xmax=x;
        if(y<rewrite_ymin) rewrite_ymin=y;
        if(y>rewrite_ymax) rewrite_ymax=y;
        rewrite_patches_x[h] = x;
        rewrite_patches_y[h] = y;
    }
    rewrite_npatchx = rewrite_xmax - rewrite_xmin + 1;
    rewrite_npatchy = rewrite_ymax - rewrite_ymin + 1;
    // Define space in file for re-writing
    hsize_t final_array_size[2], offset2[2], block2[2], count2[2];
    final_array_size[0] = params.number_of_patches[0] * params.n_space[0] + 1;
    final_array_size[1] = params.number_of_patches[1] * params.n_space[1] + 1;
    filespace = H5Screate_simple(2, final_array_size, NULL);
    offset2[0] = rewrite_xmin * params.n_space[0] + ((rewrite_xmin==0)?0:1);
    offset2[1] = rewrite_ymin * params.n_space[1] + ((rewrite_ymin==0)?0:1);
    block2 [0] = rewrite_npatchx * params.n_space[0] + ((rewrite_xmin==0)?1:0);
    block2 [1] = rewrite_npatchy * params.n_space[1] + ((rewrite_ymin==0)?1:0);
    count2 [0] = 1;
    count2 [1] = 1;
    H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset2, NULL, count2, block2);
    // Define space in memory for re-writing
    memspace = H5Screate_simple(2, block2, NULL);
    data_rewrite.resize( block2[0]*block2[1] );
    
    tmp_dset_id=0;
}

DiagnosticFields2D::~DiagnosticFields2D()
{
}


void DiagnosticFields2D::setFileSplitting( SmileiMPI* smpi, VectorPatch& vecPatches )
{
    // Calculate the total size of the array in this proc
    unsigned int total_vecPatches_size = total_patch_size * vecPatches.size();
    
    // Resize the data
    data.resize(total_vecPatches_size);
    
    // Define offset and size for HDF5 file
    hsize_t offset[1], block[1], count[1];
    offset[0] = total_patch_size * refHindex;
    block [0] = total_vecPatches_size;
    count [0] = 1;
    // Select portion of the file where this MPI will write to
    H5Sselect_hyperslab(filespace_firstwrite, H5S_SELECT_SET, offset, NULL, count, block);
    // define space in memory
    H5Sset_extent_simple(memspace_firstwrite, 1, block, block );
    
    // Create/Open temporary dataset
    htri_t status = H5Lexists(fileId_, "tmp", H5P_DEFAULT);
    if( status == 0 ) {
        hid_t pid = H5Pcreate(H5P_DATASET_CREATE);
        tmp_dset_id  = H5Dcreate( fileId_, "tmp", H5T_NATIVE_DOUBLE, filespace_firstwrite, H5P_DEFAULT, pid, H5P_DEFAULT);
        H5Pclose(pid);
    } else {
        hid_t pid = H5Pcreate(H5P_DATASET_ACCESS);
        tmp_dset_id = H5Dopen( fileId_, "tmp", pid );
        H5Pclose(pid);
    }
}


// Copy patch field to current "data" buffer
void DiagnosticFields2D::getField( Patch* patch, unsigned int field_index )
{
    // Get current field
    Field2D* field;
    if( time_average>1 ) {
        field = static_cast<Field2D*>(patch->EMfields->allFields_avg[field_index]);
    } else {
        field = static_cast<Field2D*>(patch->EMfields->allFields    [field_index]);
    }
    // Copy field to the "data" buffer
    unsigned int ix = patch_offset_in_grid[0];
    unsigned int ix_max = ix + patch_size[0];
    unsigned int iy;
    unsigned int iy_max = patch_offset_in_grid[1] + patch_size[1];
    unsigned int iout = total_patch_size * (patch->Hindex()-refHindex);
    while( ix < ix_max ) {
        iy = patch_offset_in_grid[1];
        while( iy < iy_max ) {
            data[iout] = (*field)(ix, iy);
            iout++;
            iy++;
        }
        ix++;
    }
    
    if( time_average>1 ) field->put_to(0.0);
}


// Write current buffer to file
void DiagnosticFields2D::writeField( hid_t dset_id, int timestep ) {
    
    // Write the buffer in a temporary location
    H5Dwrite( tmp_dset_id, H5T_NATIVE_DOUBLE, memspace_firstwrite, filespace_firstwrite, write_plist, &(data[0]) );
    
    // Read the file with the previously defined partition
    H5Dread( tmp_dset_id, H5T_NATIVE_DOUBLE, memspace_reread, filespace_reread, write_plist, &(data_reread[0]) );
    
    // Fold the data according to the Hilbert curve
    // TO DO: openMP-ize this loop
    unsigned int read_position, write_position, read_skip, write_skip, sx, sy;
    unsigned int write_sizey = rewrite_npatchy*(patch_size[1]-1) + ((rewrite_ymin==0)?1:0);
    read_position = 0;
    for( unsigned int h=0; h<rewrite_npatch; h++ ) {
        write_position = (rewrite_patches_y[h]-rewrite_ymin)*(patch_size[1]-1) + write_sizey*((rewrite_patches_x[h]-rewrite_xmin)*(patch_size[0]-1));
        write_skip = (rewrite_npatchy - 1)*(patch_size[1]-1);
        read_skip = 0;
        sx = patch_size[0];
        sy = patch_size[1];
        if( rewrite_patches_x[h]!=0 ) {
            read_position += patch_size[1];
            if( rewrite_xmin==0 ) write_position += write_sizey;
            sx--;
        }
        if( rewrite_patches_y[h]!=0 ) {
            read_skip++;
            if( rewrite_ymin==0 ) {
                write_position++;
                write_skip++;
            } 
            sy--;
        }
        for( unsigned int ix=0; ix<sx; ix++ ) {
            for( unsigned int iy=0; iy<sy; iy++ ) {
                data_rewrite[write_position] = data_reread[read_position];
                read_position ++;
                write_position++;
            }
            read_position  += read_skip;
            write_position += write_skip;
        }
    }
    
    // Rewrite the file with the previously defined partition
    H5Dwrite( dset_id, H5T_NATIVE_DOUBLE, memspace, filespace, write_plist, &(data_rewrite[0]) );
    
}

