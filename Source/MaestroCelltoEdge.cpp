#include <Maestro.H>
#include <Maestro_F.H>

using namespace amrex;

void 
Maestro::CelltoEdge(const RealVector& s0_cell_vec, 
                    RealVector& s0_edge_vec) 
{
    // timer for profiling
    BL_PROFILE_VAR("Maestro::CelltoEdge()", CelltoEdge);

    if (spherical) {
        Abort("Calling CelltoEdge with spherical == true");
    }

    const int max_lev = max_radial_level+1;
    get_numdisjointchunks(numdisjointchunks.dataPtr());
    get_r_start_coord(r_start_coord.dataPtr());
    get_r_end_coord(r_end_coord.dataPtr());
    get_finest_radial_level(&finest_radial_level);

    const Real * AMREX_RESTRICT s0_cell = s0_cell_vec.dataPtr();
    Real * AMREX_RESTRICT s0_edge = s0_edge_vec.dataPtr();

    for (auto n = 0; n <= finest_radial_level; ++n) {
        for (auto i = 1; i <= numdisjointchunks[n]; ++i) {
            Real nr_lev = nr[n];
            const int lo = r_start_coord[n+max_lev*i];
            const int hi = r_end_coord[n+max_lev*i]+1;
            AMREX_PARALLEL_FOR_1D(hi-lo+1, j, {
                int r = j + lo;
             
                if (r == 0) {
                    // if we are at lower domain boundary
                    s0_edge[n+max_lev*r] = s0_cell[n+max_lev*r];
                } else if (r == 1 || r == lo) {
                    // if we are at lower domain boundary+1 OR
                    // if we are at bottom of coarse-fine interface that is not a domain boundary
                    s0_edge[n+max_lev*r] = 0.5*(s0_cell[n+max_lev*(r-1)]+s0_cell[n+max_lev*r]);
                } else if (r == nr_lev) {
                    // if we are at upper domain boundary
                    s0_edge[n+max_lev*r] = s0_cell[n+max_lev*(r-1)];
                } else if (r == nr_lev-1 || r == hi) {
                    // if we are at upper domain boundary-1 OR
                    // if we are at top of coarse-fine interface that is not a domain boundary
                    s0_edge[n+max_lev*r] = 0.5*(s0_cell[n+max_lev*r]+s0_cell[n+max_lev*(r-1)]);
                } else {
                    // fourth order
                    Real tmp = 7.0/12.0 * (s0_cell[n+max_lev*r] + s0_cell[n+max_lev*(r-1)]) 
                        -1.0/12.0 * (s0_cell[n+max_lev*(r+1)] + s0_cell[n+max_lev*(r-2)]);
                    Real s0min = min(s0_cell[n+max_lev*r],s0_cell[n+max_lev*(r-1)]);
                    Real s0max = max(s0_cell[n+max_lev*r],s0_cell[n+max_lev*(r-1)]);
                    s0_edge[n+max_lev*r] = min(max(tmp,s0min),s0max);
                }
            });
        }
    }

    // make the edge values synchronous across levels
    RestrictBase(s0_edge_vec, false);
}
