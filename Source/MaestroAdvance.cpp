
#include <Maestro.H>

using namespace amrex;

// advance a single level for a single time step, updates flux registers
void
Maestro::AdvanceTimeStep (bool is_initIter)
{

    // features to be added later:
    // -delta_gamma1_term

    // MultiFabs needed within the AdvanceTimeStep routine
    Vector<std::unique_ptr<MultiFab> >        rhohalf(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >         macrhs(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >         macphi(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >       S_cc_nph(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >       thermal1(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >             s1(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >             s2(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >         s2star(finest_level+1);
    Vector<std::unique_ptr<MultiFab> > div_coeff_cart(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >   peosbar_cart(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >   delta_p_term(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >         Tcoeff(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >        hcoeff1(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >       Xkcoeff1(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >        pcoeff1(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >        hcoeff2(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >       Xkcoeff2(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >        pcoeff2(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >     scal_force(finest_level+1);
    Vector<std::unique_ptr<MultiFab> >      delta_chi(finest_level+1);

    // face-centered in the dm-direction (planar only)
    Vector<std::unique_ptr<MultiFab> >     etarhoflux(finest_level+1);

    // nodal
    Vector<std::unique_ptr<MultiFab> >    S_nodal_old(finest_level+1);

    // face-centered
    Vector<std::array< std::unique_ptr<MultiFab>, AMREX_SPACEDIM > >                umac(finest_level+1);
    Vector<std::array< std::unique_ptr<MultiFab>, AMREX_SPACEDIM > >               sedge(finest_level+1);
    Vector<std::array< std::unique_ptr<MultiFab>, AMREX_SPACEDIM > >               sflux(finest_level+1);
    Vector<std::array< std::unique_ptr<MultiFab>, AMREX_SPACEDIM > > div_coeff_cart_edge(finest_level+1);

    ////////////////////////
    // needed for spherical routines only

    Vector<std::unique_ptr<MultiFab> > w0_force_cart(finest_level+1);

    // face-centered
    Vector<std::array< std::unique_ptr<MultiFab>, AMREX_SPACEDIM > > w0mac(finest_level+1);

    // end spherical-only MultiFabs
    ////////////////////////

    Vector<Real> grav_cell_nph;
    Vector<Real> grav_cell_new;
    Vector<Real> rho0_nph;
    Vector<Real> p0_nph;
    Vector<Real> p0_minus_peosbar;
    Vector<Real> peosbar;
    Vector<Real> w0_force;
    Vector<Real> Sbar;
    Vector<Real> div_coeff_nph;
    Vector<Real> gamma1bar_temp1;
    Vector<Real> gamma1bar_temp2;
    Vector<Real> w0_old;
    Vector<Real> div_coeff_edge;
    Vector<Real> rho0_predicted_edge;
    Vector<Real> delta_chi_w0;

    constexpr int ng_s = 3;

    // wallclock time
    const Real strt_total = ParallelDescriptor::second();

    Print() << "\nTimestep " << istep << " starts with TIME = " << t_old
            << " DT = " << dt << std::endl << std::endl;

    for (int lev=0; lev<=finest_level; ++lev) 
    {
        // cell-centered MultiFabs
        rhohalf[lev].reset       (new MultiFab(grids[lev], dmap[lev],       1,    1));
        macrhs[lev].reset        (new MultiFab(grids[lev], dmap[lev],       1,    0));
        macphi[lev].reset        (new MultiFab(grids[lev], dmap[lev],       1,    1));
        S_cc_nph[lev].reset      (new MultiFab(grids[lev], dmap[lev],       1,    0));
        thermal1[lev].reset      (new MultiFab(grids[lev], dmap[lev],       1,    0));
        s1[lev].reset            (new MultiFab(grids[lev], dmap[lev],   NSCAL, ng_s));
        s2[lev].reset            (new MultiFab(grids[lev], dmap[lev],   NSCAL, ng_s));
        s2star[lev].reset        (new MultiFab(grids[lev], dmap[lev],   NSCAL, ng_s));
        div_coeff_cart[lev].reset(new MultiFab(grids[lev], dmap[lev],       1,    1));
        peosbar_cart[lev].reset  (new MultiFab(grids[lev], dmap[lev],       1,    0));
        delta_p_term[lev].reset  (new MultiFab(grids[lev], dmap[lev],       1,    0));
        Tcoeff[lev].reset        (new MultiFab(grids[lev], dmap[lev],       1,    1));
        hcoeff1[lev].reset       (new MultiFab(grids[lev], dmap[lev],       1,    1));
        Xkcoeff1[lev].reset      (new MultiFab(grids[lev], dmap[lev], NumSpec,    1));
        pcoeff1[lev].reset       (new MultiFab(grids[lev], dmap[lev],       1,    1));
        hcoeff2[lev].reset       (new MultiFab(grids[lev], dmap[lev],       1,    1));
        Xkcoeff2[lev].reset      (new MultiFab(grids[lev], dmap[lev],       1,    1));
        pcoeff2[lev].reset       (new MultiFab(grids[lev], dmap[lev], NumSpec,    1));
        scal_force[lev].reset    (new MultiFab(grids[lev], dmap[lev],   NSCAL,    1));
        delta_chi[lev].reset     (new MultiFab(grids[lev], dmap[lev],       1,    0));

        // nodal MultiFabs
        S_nodal_old[lev].reset(new MultiFab(convert(grids[lev],nodal_flag), dmap[lev], 1, 1));

        // face-centered in the dm-direction (planar only)
#if (AMREX_SPACEDIM == 2)
        etarhoflux[lev].reset(new MultiFab(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1));
#elif (AMREX_SPACEDIM == 3)
        etarhoflux[lev].reset(new MultiFab(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1));
#endif

        // face-centered arrays of MultiFabs
        umac[lev][0].reset               (new MultiFab(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1));
        umac[lev][1].reset               (new MultiFab(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1));
        sedge[lev][0].reset              (new MultiFab(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1));
        sedge[lev][1].reset              (new MultiFab(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1));
        sflux[lev][0].reset              (new MultiFab(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1));
        sflux[lev][1].reset              (new MultiFab(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1));
        div_coeff_cart_edge[lev][0].reset(new MultiFab(convert(grids[lev],nodal_flag_x), dmap[lev], 1, 1));
        div_coeff_cart_edge[lev][1].reset(new MultiFab(convert(grids[lev],nodal_flag_y), dmap[lev], 1, 1));
#if (AMREX_SPACEDIM == 3)
        umac[lev][2].reset               (new MultiFab(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1));
        sedge[lev][2].reset              (new MultiFab(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1));
        sflux[lev][2].reset              (new MultiFab(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1));
        div_coeff_cart_edge[lev][2].reset(new MultiFab(convert(grids[lev],nodal_flag_z), dmap[lev], 1, 1));
#endif
    }


    for (int lev=0; lev<=finest_level; ++lev) 
    {

        if (Verbose())
        {
            Print() << "[Level " << lev << " step " << istep+1 << "] ";
            Print() << "BEGIN ADVANCE with " << CountCells(lev) << " cells" << std::endl;
        }

        MultiFab& S_new = *snew[lev];

        const Real ctr_time = 0.5*(t_old+t_new);

        const Real* dx = geom[lev].CellSize();
        const Real* prob_lo = geom[lev].ProbLo();

        MultiFab fluxes[BL_SPACEDIM];
        if (do_reflux)
        {
            for (int i = 0; i < BL_SPACEDIM; ++i)
            {
                BoxArray ba = grids[lev];
                ba.surroundingNodes(i);
                fluxes[i].define(ba, dmap[lev], S_new.nComp(), 0);
            }
        }

        // State with ghost cells
        MultiFab Sborder(grids[lev], dmap[lev], S_new.nComp(), ng_s);
        FillPatch(lev, t_old, Sborder, sold, snew, 0, Sborder.nComp(), bcs_s);

#ifdef _OPENMP
#pragma omp parallel
#endif
        {
            FArrayBox flux[BL_SPACEDIM], uface[BL_SPACEDIM];

            for (MFIter mfi(S_new, true); mfi.isValid(); ++mfi)
            {
                const Box& bx = mfi.tilebox();

                const FArrayBox& statein = Sborder[mfi];
                FArrayBox& stateout      =   S_new[mfi];

                // Allocate fabs for fluxes and Godunov velocities.
                for (int i = 0; i < BL_SPACEDIM ; i++) {
                    const Box& bxtmp = surroundingNodes(bx,i);
                    flux[i].resize(bxtmp,S_new.nComp());
                    uface[i].resize(grow(bxtmp,1),1);
                }

                // compute velocities on faces (prescribed function of space and time)
                get_face_velocity(lev, ctr_time,
                                  AMREX_D_DECL(BL_TO_FORTRAN(uface[0]),
                                               BL_TO_FORTRAN(uface[1]),
                                               BL_TO_FORTRAN(uface[2])),
                                  dx, prob_lo);

                // compute new state (stateout) and fluxes.
                advect(bx.loVect(), bx.hiVect(),
                       BL_TO_FORTRAN_3D(statein), 
                       BL_TO_FORTRAN_3D(stateout),
                       AMREX_D_DECL(BL_TO_FORTRAN_3D(uface[0]),
                                    BL_TO_FORTRAN_3D(uface[1]),
                                    BL_TO_FORTRAN_3D(uface[2])),
                       AMREX_D_DECL(BL_TO_FORTRAN_3D(flux[0]), 
                                    BL_TO_FORTRAN_3D(flux[1]), 
                                    BL_TO_FORTRAN_3D(flux[2])), 
                       dx, dt, S_new.nComp());

                if (do_reflux) {
                    for (int i = 0; i < BL_SPACEDIM ; i++) {
                        fluxes[i][mfi].copy(flux[i],mfi.nodaltilebox(i));	  
                    }
                }
            }
        }

        // increment or decrement the flux registers by area and time-weighted fluxes
        // Note that the fluxes have already been scaled by dt and area
        // In this example we are solving s_t = -div(+F)
        // The fluxes contain, e.g., F_{i+1/2,j} = (s*u)_{i+1/2,j}
        // Keep this in mind when considering the different sign convention for updating
        // the flux registers from the coarse or fine grid perspective
        // NOTE: the flux register associated with flux_reg_s[lev] is associated
        // with the lev/lev-1 interface (and has grid spacing associated with lev-1)
        if (do_reflux) { 
            if (lev < finest_level)
            {
                for (int i = 0; i < BL_SPACEDIM; ++i) {
                    // update the lev+1/lev flux register (index lev+1)   
                    flux_reg_s[lev+1]->CrseInit(fluxes[i],i,0,0,fluxes[i].nComp(), -1.0);
                }	
            }
            if (lev > 0)
            {
                for (int i = 0; i < BL_SPACEDIM; ++i) {
                    // update the lev/lev-1 flux register (index lev) 
                    flux_reg_s[lev]->FineAdd(fluxes[i],i,0,0,fluxes[i].nComp(), 1.0);
                }
            }
        }

        if (Verbose())
        {
            Print() << "[Level " << lev << " step " << istep+1 << "] ";
            Print() << "END ADVANCE" << std::endl;
        }

    } // end loop over levels

    if (Verbose())
    {
        Print() << "Synchronizing all levels" << std::endl;
    }

    // synchronize by refluxing and averaging down, starting from the finest_level-1/finest_level pair
    for (int lev=finest_level-1; lev>=0; --lev)
    {
        if (do_reflux) {
            // update lev based on coarse-fine flux mismatch
            flux_reg_s[lev+1]->Reflux(*snew[lev], 1.0, 0, 0, snew[lev]->nComp(), geom[lev]);
        }

        AverageDownTo(lev,snew); // average lev+1 down to lev
    }

    Print() << "\nTimestep " << istep << " ends with TIME = " << t_new
            << " DT = " << dt << std::endl;

    // wallclock time
    Real end_total = ParallelDescriptor::second() - strt_total;
	
    // print wallclock time
    ParallelDescriptor::ReduceRealMax(end_total ,ParallelDescriptor::IOProcessorNumber());
    if (Verbose()) {
        Print() << "Time to advance time step: " << end_total << '\n';
    }

}

// compute the number of cells at a level
long
Maestro::CountCells (int lev)
{
    const int N = grids[lev].size();

    long cnt = 0;

#ifdef _OPENMP
#pragma omp parallel for reduction(+:cnt)
#endif
    for (int i = 0; i < N; ++i) {
        cnt += grids[lev][i].numPts();
    }

    return cnt;
}