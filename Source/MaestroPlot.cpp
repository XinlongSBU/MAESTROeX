
#include <Maestro.H>

using namespace amrex;

// write plotfile to disk
void
Maestro::WritePlotFile (const int step,
                        const Real t_in,
                        const Vector<Real>& rho0_in,
                        const Vector<Real>& p0_in,
                        const Vector<MultiFab>& u_in,
                              Vector<MultiFab>& s_in)
{
    // wallclock time
    const Real strt_total = ParallelDescriptor::second();

    const std::string& plotfilename = PlotFileName(step);

    // convert p0 to multi-D MultiFab
    Vector<MultiFab> p0_cart(finest_level+1);
    for (int lev=0; lev<=finest_level; ++lev) {
        p0_cart[lev].define(grids[lev], dmap[lev], 1, 0);
    }
    Put1dArrayOnCart(p0_in,p0_cart,0,0);

    // convert rho0 to multi-D MultiFab
    Vector<MultiFab> rho0_cart(finest_level+1);
    for (int lev=0; lev<=finest_level; ++lev) {
        rho0_cart[lev].define(grids[lev], dmap[lev], 1, 0);
    }
    Put1dArrayOnCart(rho0_in,rho0_cart,0,0);

    const auto& mf = PlotFileMF(p0_cart,rho0_cart,u_in,s_in,p0_in);
    const auto& varnames = PlotFileVarNames();

    // WriteMultiLevelPlotfile expects an array of step numbers
    Vector<int> step_array;
    step_array.resize(maxLevel()+1, step);

    WriteMultiLevelPlotfile(plotfilename, finest_level+1, mf, varnames,
                            Geom(), t_in, step_array, refRatio());

    // wallclock time
    Real end_total = ParallelDescriptor::second() - strt_total;
	
    // print wallclock time
    ParallelDescriptor::ReduceRealMax(end_total ,ParallelDescriptor::IOProcessorNumber());
    if (maestro_verbose > 0) {
        Print() << "Time to write plotfile: " << end_total << '\n';
    }

}


// get plotfile name
std::string
Maestro::PlotFileName (int lev) const
{
    return Concatenate(plot_base_name, lev, 7);
}

// put together a vector of multifabs for writing
Vector<const MultiFab*>
Maestro::PlotFileMF (const Vector<MultiFab>& p0_cart,
                     const Vector<MultiFab>& rho0_cart,
                     const Vector<MultiFab>& u_in,
                           Vector<MultiFab>& s_in,
                     const Vector<Real>& p0_in)
{

    // velocities (AMREX_SPACEDIM)
    // rho, rhoh, rhoX, tfromp, tfromh, Pi (Nscal+1)
    // X (NumSpec)
    // rho0, p0 (2)
    int nPlot = AMREX_SPACEDIM + Nscal + NumSpec + 3;

    // MultiFab to hold plotfile data
    Vector<const MultiFab*> plot_mf;

    // temporary MultiFab to hold plotfile data
    Vector<MultiFab*> plot_mf_data(finest_level+1);

    int dest_comp = 0;

    // build temporary MultiFab to hold plotfile data
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i] = new MultiFab((s_in[i]).boxArray(),(s_in[i]).DistributionMap(),nPlot,0);
    }

    // velocity
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((u_in[i]),0,dest_comp,AMREX_SPACEDIM);
    }
    dest_comp += AMREX_SPACEDIM;

    // rho
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),Rho,dest_comp,1);
    }
    ++dest_comp;

    // rhoh
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),RhoH,dest_comp,1);
    }
    ++dest_comp;

    // rhoX
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),FirstSpec,dest_comp,NumSpec);
    }
    dest_comp += NumSpec;

    // X
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),FirstSpec,dest_comp,NumSpec);
        for (int comp=0; comp<NumSpec; ++comp) {
            MultiFab::Divide(*plot_mf_data[i],s_in[i],Rho,dest_comp+comp,1,0);
        }
    }
    dest_comp += NumSpec;

    // compute tfromp
    TfromRhoP(s_in,p0_in);

    // tfromp
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),Temp,dest_comp,1);
    }
    ++dest_comp;

    // compute tfromh
    TfromRhoH(s_in,p0_in);

    for (int i = 0; i <= finest_level; ++i) {
        // tfromh
        plot_mf_data[i]->copy((s_in[i]),Temp,dest_comp,1);
    }
    ++dest_comp;

    // restore tfromp if necessary
    if (use_tfromp) {
        TfromRhoP(s_in,p0_in);
    }

    // pi
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((s_in[i]),Pi,dest_comp,1);
    }
    ++dest_comp;

    // rho0 and p0
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf_data[i]->copy((rho0_cart[i]),0,dest_comp  ,1);
        plot_mf_data[i]->copy((  p0_cart[i]),0,dest_comp+1,1);
    }
    dest_comp += 2;

    // add plot_mf_data[i] to plot_mf
    for (int i = 0; i <= finest_level; ++i) {
        plot_mf.push_back(plot_mf_data[i]);
    }

    return plot_mf;

}

// set plotfile variable names
Vector<std::string>
Maestro::PlotFileVarNames () const
{

    // velocities (AMREX_SPACEDIM)
    // rho, rhoh, rhoX, tfromp, tfromh, Pi (Nscal+1)
    // X (NumSpec)
    // rho0, p0 (2)
    int nPlot = AMREX_SPACEDIM + Nscal + NumSpec + 3;
    Vector<std::string> names(nPlot);

    int cnt = 0;

    // add velocities
    for (int i=0; i<AMREX_SPACEDIM; ++i) {
        std::string x = "vel";
        x += (120+i);
        names[cnt++] = x;
    }

    // density and enthalpy
    names[cnt++] = "rho";
    names[cnt++] = "rhoh";

    for (int i = 0; i < NumSpec; i++) {
        int len = 20;
        Vector<int> int_spec_names(len);
        //
        // This call return the actual length of each string in "len"
        //
        get_spec_names(int_spec_names.dataPtr(),&i,&len);
        char* spec_name = new char[len+1];
        for (int j = 0; j < len; j++)
            spec_name[j] = int_spec_names[j];
        spec_name[len] = '\0';
        std::string spec_string = "rhoX(";
        spec_string += spec_name;
        spec_string += ')';

        names[cnt++] = spec_string;
    }

    for (int i = 0; i < NumSpec; i++) {
        int len = 20;
        Vector<int> int_spec_names(len);
        //
        // This call return the actual length of each string in "len"
        //
        get_spec_names(int_spec_names.dataPtr(),&i,&len);
        char* spec_name = new char[len+1];
        for (int j = 0; j < len; j++) {
            spec_name[j] = int_spec_names[j];
        }
        spec_name[len] = '\0';
        std::string spec_string = "X(";
        spec_string += spec_name;
        spec_string += ')';

        names[cnt++] = spec_string;
    }

    names[cnt++] = "tfromp";
    names[cnt++] = "tfromh";
    names[cnt++] = "Pi";

    names[cnt++] = "rho0";
    names[cnt++] = "p0";

    return names;
}
