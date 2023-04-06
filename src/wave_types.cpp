#include <hydroc/wave_types.h>

// NoWave class definitions:
Eigen::VectorXd NoWave::GetForceAtTime(double t) {
    unsigned int dof = num_bodies * 6;
    Eigen::VectorXd f(dof);
    for (int i = 0; i < dof; i++) {
        f[i] = 0.0;
    }
    return f;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
// Regular wave class definitions:
RegularWave::RegularWave() {
    num_bodies = 1;
}

RegularWave::RegularWave(unsigned int num_b) {
    num_bodies = num_b;
}

void RegularWave::Initialize() {
    // set up regular waves here, call other helper functions as necessary
    int total_dofs = 6 * num_bodies;
    excitation_force_mag.resize(total_dofs);
    excitation_force_phase.resize(total_dofs);
    force.resize(total_dofs);

    double wave_omega_delta = GetOmegaDelta();
    double freq_index_des   = (regular_wave_omega / wave_omega_delta) - 1;
    for (int b = 0; b < num_bodies; b++) {
        for (int rowEx = 0; rowEx < 6; rowEx++) {
            int body_offset = 6 * b;
            // why are these always 0? vvv TODO check/change this
            excitation_force_mag[body_offset + rowEx]   = GetExcitationMagInterp(b, rowEx, 0, freq_index_des);
            excitation_force_phase[body_offset + rowEx] = GetExcitationPhaseInterp(b, rowEx, 0, freq_index_des);
        }
    }
}

void RegularWave::AddH5Data(std::vector<HydroData::RegularWaveInfo>& reg_h5_data) {
    info = reg_h5_data;
}

// should return a 6N long vector
Eigen::VectorXd RegularWave::GetForceAtTime(double t) {
    unsigned int dof = num_bodies * 6;
    Eigen::VectorXd f(dof);
    // initialize the force here:
    for (int b = 0; b < num_bodies; b++) {
        int body_offset = 6 * b;
        for (int rowEx = 0; rowEx < 6; rowEx++) {
            f[body_offset + rowEx] = excitation_force_mag[body_offset + rowEx] * regular_wave_amplitude *
                                     cos(regular_wave_omega * t + excitation_force_phase[rowEx]);
        }
    }
    return f;
}

// put more reg wave forces here:
// helper GetOmegaDelta()
/*******************************************************************************
 * RegularWave::GetOmegaDelta()
 * returns omega step size
 *******************************************************************************/
double RegularWave::GetOmegaDelta() const {
    double omega_max = info[0].freq_list[info[0].freq_list.size() - 1];
    double num_freqs = info[0].freq_list.size();
    return omega_max / num_freqs;
}

/*******************************************************************************
 * RegularWave::GetExcitationMagInterp()
 * returns excitation magnitudes for body b, row i, column j, frequency ix k
 *******************************************************************************/
double RegularWave::GetExcitationMagInterp(int b, int i, int j, double freq_index_des) const {
    double freq_interp_val    = freq_index_des - floor(freq_index_des);
    double excitationMagFloor = info[b].excitation_mag_matrix(i, j, (int)floor(freq_index_des));
    double excitationMagCeil  = info[b].excitation_mag_matrix(i, j, (int)floor(freq_index_des) + 1);
    double excitationMag      = (freq_interp_val * (excitationMagCeil - excitationMagFloor)) + excitationMagFloor;

    return excitationMag;
}

/*******************************************************************************
 * RegularWave::GetExcitationPhaseInterp()
 * returns excitation phases for row i, column j, frequency ix k
 *******************************************************************************/
double RegularWave::GetExcitationPhaseInterp(int b, int i, int j, double freq_index_des) const {
    double freq_interp_val      = freq_index_des - floor(freq_index_des);  // look into c++ modf TODO
    double excitationPhaseFloor = info[b].excitation_phase_matrix(
        i, j, (int)floor(freq_index_des));  // TODO check if freq_index_des is >0, if so just cast instead of floor
    double excitationPhaseCeil = info[b].excitation_phase_matrix(i, j, (int)floor(freq_index_des) + 1);
    double excitationPhase = (freq_interp_val * (excitationPhaseCeil - excitationPhaseFloor)) + excitationPhaseFloor;

    return excitationPhase;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

// Irregular wave class definitions:
IrregularWave::IrregularWave() {
    num_bodies = 1;
}

IrregularWave::IrregularWave(unsigned int num_b) {
    num_bodies = num_b;
}

void IrregularWave::Initialize() {
    std::vector<Eigen::MatrixXd> ex_irf_old(num_bodies);
    std::vector<Eigen::VectorXd> ex_irf_time_old(num_bodies);
    for (int b = 0; b < num_bodies; b++) {
        ex_irf_old[b]      = GetExcitationIRF(b);
        ex_irf_time_old[b] = info[b].excitation_irf_time;
    }

    // resample excitation IRF time series
    // h5 file irf has different timestep, want to resample with interpolation (cubic spline?) once at start no
    // interpolation in convolution integral part
    // different one for each body it's 6x1x1000 so maybe switch to 2d reading
    ex_irf_resampled.resize(num_bodies);
    ex_irf_time_resampled.resize(num_bodies);
    for (int b = 0; b < num_bodies; b++) {  // function call (time_in, vals_in, &time_out, &vals_out)
        ex_irf_time_resampled[b] = ResampleTime(ex_irf_time_old[b], simulation_dt);
        ex_irf_resampled[b]      = ResampleVals(ex_irf_time_old[b], ex_irf_old[b], ex_irf_time_resampled[b]);
    }

    // TODO initialize eta actually
    //eta.resize(ex_irf_time_resampled[0].size());
    //for (int i = 0; i < ex_irf_time_resampled[0].size(); i++) {
    //    eta[i] = 0.0;
    //}
    // TODO check that spectrum_frequencies is set here? maybe just before using it
    CreateSpectrum();
    CreateFreeSurfaceElevation(); // eta initialized in here
}

#include <unsupported/Eigen/Splines>

Eigen::MatrixXd IrregularWave::ResampleVals(const Eigen::VectorXd& t_old,
                                            Eigen::MatrixXd& vals_old,
                                            const Eigen::VectorXd& t_new) {
    assert(vals_old.rows() == 6);

    Eigen::MatrixXd vals_new(6, t_new.size());
    // we need to ensure the old times used start at 0, since the new times will
    double dt_old                 = t_old[1] - t_old[0];
    Eigen::VectorXd t_old_shifted = Eigen::VectorXd::LinSpaced(t_old.size(), 0, (t_old.size() - 1) * dt_old);

    // interpolate the irf over each dof separately, 1 row at a time
    for (int dof = 0; dof < 6; dof++) {
        Eigen::Spline<double, 1> spline =
            Eigen::SplineFitting<Eigen::Spline<double, 1>>::Interpolate(vals_old.row(dof), 3, t_old_shifted);
        for (int i = 0; i < t_new.size(); i++) {
            vals_new(dof, i) = spline(t_new[i])[0];
        }
    }

    std::ofstream out("resample.txt");
    // print for testing if it gets this far:
    for (int i = 0; i < t_new.size(); i++) {
        out << t_new[i];
        for (int dof = 0; dof < 6; dof++) {
            out << " " << vals_new(dof, i);
        }
        out << std::endl;
    }
    out.close();
    out.open("compare.txt");
    for (int i = 0; i < t_old.size(); i++) {
        out << t_old[i];
        for (int dof = 0; dof < 6; dof++) {
            out << " " << vals_old(dof, i);
        }
        out << std::endl;
    }
    out.close();

    return vals_new;
}

Eigen::VectorXd IrregularWave::ResampleTime(const Eigen::VectorXd& t_old, const double dt_new) {
    double dt_old  = t_old[1] - t_old[0];
    int size_new   = static_cast<int>(ceil(t_old.size() * dt_old / dt_new));
    double t_final = (t_old.size() - 1) * dt_old;

    Eigen::VectorXd t_new = Eigen::VectorXd::LinSpaced(size_new, 0, t_final);

    // print for testing
    int N = t_old.size() - 1;
    std::cout << "T_old = {t_i | i = 0 .. " << N << ", t_0 = " << t_old[0] << ", t_" << N << " = " << t_old[N] << "}\n"
              << "dt_old = " << dt_old << "\nT_new = {t_i | i = 0 .. " << N << ", t_0 = " << t_new[0] << ", t_" << N
              << " = " << t_new[N] << "}\n"
              << "dt_new = " << dt_new << std::endl;

    return t_new;
}

void IrregularWave::AddH5Data(std::vector<HydroData::IrregularWaveInfo>& irreg_h5_data) {
    info = irreg_h5_data;
}

Eigen::VectorXd IrregularWave::GetForceAtTime(double t) {
    unsigned int total_dofs = num_bodies * 6;
    Eigen::VectorXd f(total_dofs);
    // initialize the force here:
    // for now set f to all zeros
    for (int i = 0; i < total_dofs; i++) {
        f[i] = 0.0;
    }
    // see ComputeForceExcitation and convolution functions

    // force_excitation.resize(total_dofs, 0.0);

    for (int body = 0; body < num_bodies; body++) {
        // Loop through the DOFs
        for (int dof = 0; dof < 6; ++dof) {
            // Compute the convolution for the current DOF
            double f_dof          = ExcitationConvolution(body, dof, t);
            unsigned int b_offset = body * 6;
            f[b_offset + dof]     = f_dof;
        }
    }

    return f;
}

/*******************************************************************************
 * IrregularWave::GetExcitationIRF()
 * returns the std::vector of excitation_irf_matrix from h5 file
 *******************************************************************************/
Eigen::MatrixXd IrregularWave::GetExcitationIRF(int b) const {
    return info[b].excitation_irf_matrix;
}

double IrregularWave::ExcitationConvolution(int body, int dof, double time) {
    double f_ex = 0.0;

    for (size_t j = 0; j < ex_irf_time_resampled[0].size(); ++j) {
        double tau        = ex_irf_time_resampled[0][j];
        double t_tau      = time - tau;
        double ex_irf_val = ex_irf_resampled[body](dof, j);  // needs to be resampled version TODO
        if (0.0 < t_tau && t_tau < eta.size() * simulation_dt) {
            size_t eta_index = static_cast<size_t>(t_tau / simulation_dt);
            double eta_val   = eta[eta_index - 1];
            f_ex += ex_irf_val * eta_val * simulation_dt;  // eta is wave elevation
        }
    }

    return f_ex;
}

Eigen::VectorXd IrregularWave::SetSpectrumFrequencies(double start, double end, int num_points) {
    Eigen::VectorXd result(num_points);
    double step = (end - start) / (num_points - 1);

    for (int i = 0; i < num_points; ++i) {
        result[i] = start + i * step;
    }

    spectrum_frequencies = result;

    return result;
}

void IrregularWave::CreateSpectrum() {
    // Define the frequency vector
    //spectrum_frequencies = Linspace(0.001, 1.0, 1000);  

    // Calculate the Pierson-Moskowitz Spectrum
    spectral_densities = PiersonMoskowitzSpectrumHz(spectrum_frequencies, wave_height, wave_period);

    // Open a file stream for writing
    std::ofstream outputFile("spectral_densities.txt");

    // Check if the file stream is open
    if (outputFile.is_open()) {
        // Write the spectral densities and their corresponding frequencies to the file
        for (size_t i = 0; i < spectral_densities.size(); ++i) {
            outputFile << spectrum_frequencies[i] << " : " << spectral_densities[i] << std::endl;
        }

        // Close the file stream
        outputFile.close();
    } else {
        std::cerr << "Unable to open file for writing." << std::endl;
    }
}

// TODO put spectrum functions in a new namespace (when we have more options?)
Eigen::VectorXd PiersonMoskowitzSpectrumHz(Eigen::VectorXd& f, double Hs, double Tp) {
    // Sort the frequency vector
    std::sort(f.begin(), f.end());

    // Initialize the spectral densities vector
    Eigen::VectorXd spectral_densities(f.size());

    // Calculate the spectral densities
    for (size_t i = 0; i < f.size(); ++i) {
        spectral_densities[i] = 1.25 * std::pow(1 / Tp, 4) * std::pow(Hs / 2, 2) * std::pow(f[i], -5) *
                                std::exp(-1.25 * std::pow(1 / Tp, 4) * std::pow(f[i], -4));
    }

    return spectral_densities;
}

void IrregularWave::CreateFreeSurfaceElevation() {
    // Create a time index vector
    //UpdateNumTimesteps();
    int num_timesteps = static_cast<int>(simulation_duration / simulation_dt) + 1;

    Eigen::VectorXd time_index = Eigen::VectorXd::LinSpaced(num_timesteps, 0, simulation_duration);

    // Calculate the surface elevation
    eta = FreeSurfaceElevation(spectrum_frequencies, spectral_densities, time_index);

    // Apply ramp if ramp_duration is greater than 0
    //if (ramp_duration > 0.0) {
    //    UpdateRampTimesteps();
    //    ramp_timesteps = static_cast<int>(ramp_duration / simulation_dt) + 1;
    //    ramp           = Linspace(0.0, 1.0, ramp_timesteps);

    //    for (size_t i = 0; i < ramp.size(); ++i) {
    //        eta[i] *= ramp[i];
    //    }
    //}

    // Open a file stream for writing
    std::ofstream eta_output("eta.txt");
    // Check if the file stream is open
    if (eta_output.is_open()) {
        // Write the spectral densities and their corresponding frequencies to the file
        for (size_t i = 0; i < eta.size(); ++i) {
            eta_output << time_index[i] << " : " << eta[i] << std::endl;
        }
        // Close the file stream
        eta_output.close();
    } else {
        std::cerr << "Unable to open file for writing." << std::endl;
    }

    //std::vector<std::array<double, 3>> free_surface_3d_pts    = CreateFreeSurface3DPts(eta, time_index);
    //std::vector<std::array<size_t, 3>> free_surface_triangles = CreateFreeSurfaceTriangles(time_index.size());

    //WriteFreeSurfaceMeshObj(free_surface_3d_pts, free_surface_triangles, "fse_mesh.obj");
}

Eigen::VectorXd FreeSurfaceElevation(const Eigen::VectorXd& freqs_hz,
                                         const Eigen::VectorXd& spectral_densities,
                                         const Eigen::VectorXd& time_index,
                                         int seed) {
    double delta_f = freqs_hz(Eigen::last) / freqs_hz.size();
    std::vector<double> omegas(freqs_hz.size());

    for (size_t i = 0; i < freqs_hz.size(); ++i) {
        omegas[i] = 2 * M_PI * freqs_hz[i];
    }

    std::vector<double> A(spectral_densities.size());
    for (size_t i = 0; i < spectral_densities.size(); ++i) {
        A[i] = 2 * spectral_densities[i] * delta_f;
    }

    std::vector<double> sqrt_A(A.size());
    for (size_t i = 0; i < A.size(); ++i) {
        sqrt_A[i] = std::sqrt(A[i]);
    }
    // TODO fix this vector of vecotrs
    std::vector<std::vector<double>> omegas_t(time_index.size(), std::vector<double>(omegas.size()));
    for (size_t i = 0; i < time_index.size(); ++i) {
        for (size_t j = 0; j < omegas.size(); ++j) {
            omegas_t[i][j] = time_index[i] * omegas[j];
        }
    }

    std::mt19937 rng(seed);  // Creates an instance of the std::mt19937 random number generator; a Mersenne Twister
                             // random number engine. The seed parameter is used to initialize the generator's internal
                             // state - to control the random sequence produced.
    std::uniform_real_distribution<double> dist(0.0, 2 * M_PI);
    std::vector<double> phases(omegas.size());
    for (size_t i = 0; i < phases.size(); ++i) {
        phases[i] = dist(rng);
    }

    Eigen::VectorXd eta(time_index.size());
    eta.setZero(time_index.size());
    for (size_t i = 0; i < spectral_densities.size(); ++i) {
        for (size_t j = 0; j < time_index.size(); ++j) {
            eta[j] += sqrt_A[i] * std::cos(omegas_t[j][i] + phases[i]);
        }
    }

    return eta;
}