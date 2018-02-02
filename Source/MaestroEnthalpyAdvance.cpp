
#include <Maestro.H>

using namespace amrex;

void
Maestro::EnthalpyAdvance (bool is_predictor,
                          Vector<MultiFab>& scalold,
                          Vector<MultiFab>& scalnew,
                          Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sedge,
                          Vector<std::array< MultiFab, AMREX_SPACEDIM > >& sflux,
                          Vector<MultiFab>& scal_force,
                          Vector<std::array< MultiFab, AMREX_SPACEDIM > >& umac)
{
    // Create edge-centered base state quantities.
    // Note: rho0_edge_{old,new} and rhoh0_edge_{old,new}
    // contain edge-centered quantities created via spatial interpolation.
    Vector<Real>  rho0_edge_old( (max_radial_level+1)*(nr_fine+1) );
    Vector<Real>  rho0_edge_new( (max_radial_level+1)*(nr_fine+1) );
    Vector<Real> rhoh0_edge_old( (max_radial_level+1)*(nr_fine+1) );
    Vector<Real> rhoh0_edge_new( (max_radial_level+1)*(nr_fine+1) );

    if (spherical == 0) {
        cell_to_edge( rho0_old.dataPtr(), rho0_edge_old.dataPtr());
        cell_to_edge( rho0_new.dataPtr(), rho0_edge_new.dataPtr());
        cell_to_edge(rhoh0_old.dataPtr(),rhoh0_edge_old.dataPtr());
        cell_to_edge(rhoh0_new.dataPtr(),rhoh0_edge_new.dataPtr());
    }

    if (enthalpy_pred_type == predict_h ||
        enthalpy_pred_type == predict_hprime) {
        // convert (rho h) -> h
        ConvertRhoHToH(sold,true);
    }

    //////////////////////////////////
    // Create scalar source term at time n
    //////////////////////////////////

    for (int lev=0; lev<=finest_level; ++lev) {
        scal_force[lev].setVal(0.);
    }

    // compute forcing terms    
    if (enthalpy_pred_type == predict_rhohprime) {
        // make force for (rho h)'
        Abort("MaestroEnthalpyAdavnce FIXME rhoh' force");



    }
    else if (enthalpy_pred_type == predict_h ||
             enthalpy_pred_type == predict_rhoh) {
        // make force for h by calling mkrhohforce then dividing by rho
        Abort("MaestroEnthalpyAdavnce forcing");
    }
    else if (enthalpy_pred_type == predict_hprime) {
        // first compute h0_old
        // make force for hprime
        Abort("MaestroEnthalpyAdavnce forcing");
    }
    else if (enthalpy_pred_type == predict_T_then_rhohprime ||
             enthalpy_pred_type == predict_T_then_h ||
             enthalpy_pred_type == predict_Tprime_then_h) {
        // make force for temperature
        Abort("MaestroEnthalpyAdavnce forcing");
    }
    

    //////////////////////////////////
    // Add w0 to MAC velocities
    //////////////////////////////////

    Addw0(umac,1.);

    //////////////////////////////////
    // Create the edge states of (rho h)' or h or T 
    //////////////////////////////////

    if (enthalpy_pred_type == predict_rhohprime) {
        // convert (rho h) -> (rho h)'
        PutInPertForm(scalold, rhoh0_old, RhoH, 0, bcs_f, true);
    }

    if (enthalpy_pred_type == predict_hprime) {
        // convert h -> h'
        Abort("MaestroEnthalpyAdavnce predict_hprime");
    }

    if (enthalpy_pred_type == predict_Tprime_then_h) {
        // convert T -> T'
        Abort("MaestroEnthalpyAdavnce predict_Tprime_then_h");
    }
    
    // predict either T, h, or (rho h)' at the edges
    int pred_comp;
    if ( enthalpy_pred_type == predict_T_then_rhohprime ||
         enthalpy_pred_type == predict_T_then_h         ||
         enthalpy_pred_type == predict_Tprime_then_h)  {
        pred_comp = Temp;
    }
    else {
        pred_comp = RhoH;
    }

    if (enthalpy_pred_type == predict_rhoh) {
        // use the conservative form of the prediction
        Abort("MaestroEnthalpyAdavnce predict_rhoh");
    }
    else {
        // use the advective form of the prediction
        MakeEdgeScal(scalold,sedge,umac,scal_force,0,bcs_s,Nscal,pred_comp,pred_comp,1,0);
    }


}