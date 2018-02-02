
#include <Maestro.H>

using namespace amrex;

void
Maestro::DensityAdvance (bool is_predictor,
                         Vector<MultiFab>& scalold,
                         Vector<MultiFab>& scalnew,
                         Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                         Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sflux,
                         Vector<MultiFab>& scal_force,
                         Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac)
{
    
    Vector<Real> rho0_edge_old( (max_radial_level+1)*(nr_fine+1) );
    Vector<Real> rho0_edge_new( (max_radial_level+1)*(nr_fine+1) );

    if (spherical == 0) {
        // create edge-centered base state quantities.
        // Note: rho0_edge_{old,new} 
        // contains edge-centered quantities created via spatial interpolation.
        // This is to be contrasted to rho0_predicted_edge which is the half-time
        // edge state created in advect_base. 
        cell_to_edge(rho0_old.dataPtr(),rho0_edge_old.dataPtr());
        cell_to_edge(rho0_new.dataPtr(),rho0_edge_new.dataPtr());
    }

    //////////////////////////////////
    // Create source terms at time n
    //////////////////////////////////

    // source terms for X and for tracers are zero - do nothing
    for (int lev=0; lev<=finest_level; ++lev) {
        scal_force[lev].setVal(0.);
    }

    if (spherical == 1) {

    }

    // ** density source term **

    // Make source term for rho or rho' 
    if (species_pred_type == predict_rhoprime_and_X) {
        // rho' souce term
        // this is needed for pred_rhoprime_and_X
        ModifyScalForce(scal_force,umac,rho0_old,rho0_edge_old,Rho,bcs_s,0);

    }
    else if (species_pred_type == predict_rho_and_X) {
        // rho source term
        ModifyScalForce(scal_force,umac,rho0_old,rho0_edge_old,Rho,bcs_s,1);

    }

    // ** species source term **

    // for species_pred_types predict_rhoprime_and_X and
    // predict_rho_and_X, there is no force for X.

    // for predict_rhoX, we are predicting (rho X)
    // as a conservative equation, and there is no force.


    /////////////////////////////////////////////////////////////////
    // Add w0 to MAC velocities (trans velocities already have w0).
    /////////////////////////////////////////////////////////////////

    Addw0(umac,1.);
    
    /////////////////////////////////////////////////////////////////
    // Create the edge states of (rho X)' or X and rho'
    /////////////////////////////////////////////////////////////////

    if ((species_pred_type == predict_rhoprime_and_X) ||
        (species_pred_type == predict_rho_and_X)) {

        // we are predicting X to the edges, so convert the scalar
        // data to those quantities

        // convert (rho X) --> X in scalold 
	ConvertRhoXToX(scalold,true);
    }

    if (species_pred_type == predict_rhoprime_and_X) {
        // convert rho -> rho' in scalold
        //   . this is needed for predict_rhoprime_and_X
	PutInPertForm(scalold, rho0_old, Rho, 0, bcs_f, true);
    }

    // predict species at the edges -- note, either X or (rho X) will be
    // predicted here, depending on species_pred_type

    int is_vel = 0; // false
    if (species_pred_type == predict_rhoprime_and_X) {

	// we are predicting X to the edges, using the advective form of
	// the prediction
	// call make_edge_scal(sold,sedge,umac,scal_force, &
	//                     dx,dt,is_vel,the_bc_level, &
	//                     spec_comp,dm+spec_comp,nspec,.false.,mla)
	MakeEdgeScal(scalold,sedge,umac,scal_force,is_vel,bcs_s,Nscal,FirstSpec,FirstSpec,NumSpec,0);
    }
   
    // predict rho or rho' at the edges (depending on species_pred_type)
    if (species_pred_type == predict_rhoprime_and_X) {
	// call make_edge_scal(sold,sedge,umac,scal_force, &
	//                     dx,dt,is_vel,the_bc_level, &
	//                     rho_comp,dm+rho_comp,1,.false.,mla)
	MakeEdgeScal(scalold,sedge,umac,scal_force,is_vel,bcs_s,Nscal,Rho,Rho,1,0);
    }
    
    if (species_pred_type == predict_rhoprime_and_X) {
	// convert rho' -> rho in scalold 
	PutInPertForm(scalold, rho0_old, Rho, Rho, bcs_s, false);
    }

    if ((species_pred_type == predict_rhoprime_and_X) ||
        (species_pred_type == predict_rho_and_X)) {
	// convert X --> (rho X) in scalold 
	ConvertRhoXToX(scalold,false);
    }

 
    /////////////////////////////////////////////////////////////////
    // Subtract w0 from MAC velocities.
    /////////////////////////////////////////////////////////////////

    Addw0(umac,-1.);


    /////////////////////////////////////////////////////////////////
    // Compute fluxes
    /////////////////////////////////////////////////////////////////

    if (is_predictor == 1) {
	
	// compute species fluxes
	// call mk_rhoX_flux(mla,sflux,etarhoflux,sold,sedge,umac,w0,w0mac, &
        //                  rho0_old,rho0_edge_old,rho0mac_old, &
        //                  rho0_old,rho0_edge_old,rho0mac_old, &
        //                  rho0_predicted_edge,spec_comp,spec_comp+nspec-1)
	MakeRhoXFlux(scalold, sflux, sedge, umac,
		     rho0_old,rho0_edge_old, 
		     rho0_old,rho0_edge_old,
		     FirstSpec,NumSpec);

    } else if (is_predictor == 2) {
	// compute species fluxes
	// call mk_rhoX_flux(mla,sflux,etarhoflux,sold,sedge,umac,w0,w0mac, &
	//                   rho0_old,rho0_edge_old,rho0mac_old, &
	//                   rho0_new,rho0_edge_new,rho0mac_new, &
	//                   rho0_predicted_edge,spec_comp,spec_comp+nspec-1)
	MakeRhoXFlux(scalold, sflux, sedge, umac,
		     rho0_old,rho0_edge_old, 
		     rho0_new,rho0_edge_new,
		     FirstSpec,NumSpec);
    }

    //**************************************************************************
    //     1) Set force for (rho X)_i at time n+1/2 = 0.
    //     2) Update (rho X)_i with conservative differencing.
    //     3) Define density as the sum of the (rho X)_i
    //     4) Update tracer with conservative differencing as well.
    //**************************************************************************
    
    for (int lev=0; lev<=finest_level; ++lev) {
	scal_force[lev].setVal(0.);
    }

    // p0 only used in rhoh update so we just pass in a dummy version
    // call update_scal(mla,spec_comp,spec_comp+nspec-1,sold,snew,sflux,scal_force, &
    //                  p0_dummy,p0_dummy_cart,dx,dt,the_bc_level)
    

    // if (verbose >= 1) {
    // 	Real smin, smax;
    // 	for (int lev=0; lev<=finest_level; ++lev) {
    // 	    // if (parallel_IOProcessor()) Print() << lev << endl;

    // 	    for (int comp = FirstSpec; comp < FirstSpec+NumSpec; ++comp) {
    // 		MultiFab::Divide(snew[lev],snew[lev],Rho,comp,1,0);
             
    // 		smin = snew[lev].min(comp); 
    // 		smax = snew[lev].max(comp);
             
    //          // if (parallel_IOProcessor()) 
    // 	     // 	 Print() << "Last species: " << smin << " " << smax << endl;

    // 		MultiFab::Multiply(snew[lev],snew[lev],Rho,comp,1,0);
    // 	    }
	    
    // 	    smin = snew[lev].min(Rho); 
    // 	    smax = snew[lev].max(Rho);
          
    // 	    // if (parallel_IOProcessor()) 
    // 	    // 	Print() << "Rho: " << smin << " " << smax << endl;
    // 	}
    // }


// Print() << "... Level "<< i1 << " update:" << endl;
// Print() << "... new min/max : density           " << endl;
// 2002 format('... new min/max : ',a16,2x,e17.10,2x,e17.10)
// 2003 format('... new min/max : tracer            ',e17.10,2x,e17.10)

}