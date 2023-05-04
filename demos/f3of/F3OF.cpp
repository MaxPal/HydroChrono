#include <hydroc/gui/guihelper.h>
#include <hydroc/helper.h>
#include <hydroc/hydro_forces.h>

#include <chrono/core/ChRealtimeStep.h>
#include <chrono/physics/ChLinkMate.h>

#include <chrono>   // std::chrono::high_resolution_clock::now
#include <iomanip>  // std::setprecision
#include <vector>   // std::vector<double>

// Use the namespaces of Chrono
using namespace chrono;
using namespace chrono::geometry;

// usage: ./<demos>.exe [DATADIR] [--nogui]
//
// If no argument is given user can set HYDROCHRONO_DATA_DIR
// environment variable to give the data_directory.
//
int main(int argc, char* argv[]) {
    GetLog() << "Chrono version: " << CHRONO_VERSION << "\n\n";

    if (hydroc::setInitialEnvironment(argc, argv) != 0) {
        return 1;
    }

    // Check if --nogui option is set as 2nd argument
    bool visualizationOn = true;
    if (argc > 2 && std::string("--nogui").compare(argv[2]) == 0) {
        visualizationOn = false;
    }

    // Get model file names
    std::filesystem::path DATADIR(hydroc::getDataDir());

    auto body1_meshfame = (DATADIR / "f3of" / "geometry" / "base.obj").lexically_normal().generic_string();
    auto body2_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto body3_meshfame = (DATADIR / "f3of" / "geometry" / "flap.obj").lexically_normal().generic_string();
    auto h5fname        = (DATADIR / "f3of" / "hydroData" / "f3of.h5").lexically_normal().generic_string();

    // system/solver settings
    ChSystemSMC system;

    system.Set_G_acc(ChVector<>(0.0, 0.0, -9.81));
    double timestep = 0.02;
    system.SetSolverType(ChSolver::Type::SPARSE_QR);
    system.SetSolverMaxIterations(300);  // the higher, the easier to keep the constraints satisfied.
    system.SetStep(timestep);
    ChRealtimeStepTimer realtime_timer;
    double simulationDuration = 300.0;

    // Create user interface
    std::shared_ptr<hydroc::gui::UI> pui = hydroc::gui::CreateUI(visualizationOn);
    hydroc::gui::UI& ui                  = *pui.get();

    // some io/viz options
    bool profilingOn = true;
    bool saveDataOn  = true;
    std::vector<double> time_vector;
    std::vector<double> base_surge;
    std::vector<double> base_pitch;
    std::vector<double> fore_pitch;
    std::vector<double> aft_pitch;

    // set up body from a mesh
    std::cout << "Attempting to open mesh file: " << body1_meshfame << std::endl;
    std::shared_ptr<ChBody> base = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body1_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto red = chrono_types::make_shared<ChVisualMaterial>();
    red->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.1f));
    base->GetVisualShape(0)->SetMaterial(0, red);

    // define the base's initial conditions (position and rotation defined later for specific test)
    system.Add(base);
    base->SetNameString("body1");
    base->SetMass(1089825.0);
    base->SetInertiaXX(ChVector<>(100000000.0, 76300000.0, 100000000.0));

    std::cout << "Attempting to open mesh file: " << body2_meshfame << std::endl;
    std::shared_ptr<ChBody> flapFore = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body2_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto blue = chrono_types::make_shared<ChVisualMaterial>();
    blue->SetDiffuseColor(ChColor(0.3f, 0.1f, 0.6f));
    flapFore->GetVisualShape(0)->SetMaterial(0, blue);

    // define the fore flap's initial conditions (position and rotation defined later for specific tests
    system.Add(flapFore);
    flapFore->SetNameString("body2");
    flapFore->SetMass(179250.0);
    flapFore->SetInertiaXX(ChVector<>(100000000.0, 1300000.0, 100000000.0));

    std::cout << "Attempting to open mesh file: " << body3_meshfame << std::endl;
    std::shared_ptr<ChBody> flapAft = chrono_types::make_shared<ChBodyEasyMesh>(  //
        body3_meshfame,
        0,      // density
        false,  // do not evaluate mass automatically
        true,   // create visualization asset
        false   // collisions
    );

    // Create a visualization material
    auto green = chrono_types::make_shared<ChVisualMaterial>();
    green->SetDiffuseColor(ChColor(0.3f, 0.6f, 0.1f));
    flapAft->GetVisualShape(0)->SetMaterial(0, green);

    // define the aft flap's initial conditions (position and rotation defined later for specific tests
    system.Add(flapAft);
    flapAft->SetNameString("body3");
    flapAft->SetMass(179250.0);
    flapAft->SetInertiaXX(ChVector<>(100000000.0, 1300000.0, 100000000.0));

    // ---------------- Begin specific DT set up, comment out unused tests ----------------------------
    // ---------------- DT1 set up (surge decay, flaps locked, no waves) ------------------------------
    //// set up pos/rotations
    // base->SetPos(ChVector<>(5.0, 0.0, -9.0));
    // flapFore->SetPos( ChVector<>(5.0 + -12.5, 0.0, -9.0 + 3.5) );
    // flapAft->SetPos( ChVector<>(5.0 + 12.5, 0.0, -9.0 + 3.5) );
    //// set up revolute joints and lock them
    // auto revoluteFore = chrono_types::make_shared<ChLinkLockRevolute>();
    // auto revoluteAft = chrono_types::make_shared<ChLinkLockRevolute>();
    // ChQuaternion<> revoluteRot = Q_from_AngX(CH_C_PI / 2.0); // do not change
    // revoluteFore->Initialize(base, flapFore, ChCoordsys<>(ChVector<>(5.0 - 12.5, 0.0, -9.0), revoluteRot));
    // system.AddLink(revoluteFore);
    // revoluteAft->Initialize(base, flapAft, ChCoordsys<>(ChVector<>(5.0 + 12.5, 0.0, -9.0), revoluteRot));
    // system.AddLink(revoluteAft);
    // revoluteFore->Lock(true);
    // revoluteAft->Lock(true);
    //// create ground
    // auto ground = chrono_types::make_shared<ChBody>();
    // system.AddBody(ground);
    // ground->SetPos(ChVector<>(0, 0, -9.0));
    // ground->SetIdentifier(-1);
    // ground->SetBodyFixed(true);
    // ground->SetCollide(false);
    //// add prismatic joint between the base and ground
    // auto prismatic = chrono_types::make_shared<ChLinkLockPrismatic>();
    // prismatic->Initialize(ground, base, ChCoordsys<>(ChVector<>(0.0, 0.0, -9.0), Q_from_AngY(CH_C_PI_2)));
    // system.AddLink(prismatic);
    //// add damping to prismatic joint
    // auto prismatic_pto = chrono_types::make_shared<ChLinkTSDA>();
    // prismatic_pto->Initialize(ground, base, true, ChVector<>(0.0, 0.0, 0.0), ChVector<>(0.0, 0.0, 0.0));
    // prismatic_pto->SetSpringCoefficient(1e5);
    // prismatic_pto->SetRestLength(0.0);
    // system.AddLink(prismatic_pto);
    // ---------------- DT2 set up (flaps locked, base pitch decay, no waves) ---------------------------------
    // adjust initial pitch here only, rotations and positions calcuated from this:
    double ang_rad = CH_C_PI / 18.0;
    // set up pos/rotations
    base->SetPos(ChVector<>(0.0, 0.0, -9.0));
    base->SetRot(Q_from_AngAxis(ang_rad, VECT_Y));
    flapFore->SetRot(Q_from_AngAxis(ang_rad, VECT_Y));
    flapAft->SetRot(Q_from_AngAxis(ang_rad, VECT_Y));
    flapFore->SetPos(ChVector<>(-12.5 * std::cos(ang_rad) + 3.5 * std::sin(ang_rad), 0.0,
                                -9.0 + 12.5 * std::sin(ang_rad) + 3.5 * std::cos(ang_rad)));
    flapAft->SetPos(ChVector<>(12.5 * std::cos(ang_rad) + 3.5 * std::sin(ang_rad), 0.0,
                               -9.0 - 12.5 * std::sin(ang_rad) + 3.5 * std::cos(ang_rad)));

    // set up revolute joints and lock them
    auto revoluteFore          = chrono_types::make_shared<ChLinkLockRevolute>();
    auto revoluteAft           = chrono_types::make_shared<ChLinkLockRevolute>();
    ChQuaternion<> revoluteRot = Q_from_AngX(CH_C_PI / 2.0);  // do not change
    revoluteFore->Initialize(
        base, flapFore,
        ChCoordsys<>(ChVector<>(-12.5 * std::cos(ang_rad), 0.0, -9.0 + 12.5 * std::sin(ang_rad)), revoluteRot));
    system.AddLink(revoluteFore);
    revoluteAft->Initialize(
        base, flapAft,
        ChCoordsys<>(ChVector<>(12.5 * std::cos(ang_rad), 0.0, -9.0 - 12.5 * std::sin(ang_rad)), revoluteRot));
    system.AddLink(revoluteAft);
    revoluteFore->Lock(true);
    revoluteAft->Lock(true);
    // create ground
    auto ground = chrono_types::make_shared<ChBody>();
    system.AddBody(ground);
    ground->SetPos(ChVector<>(0, 0, -9.0));
    ground->SetIdentifier(-1);
    ground->SetBodyFixed(true);
    ground->SetCollide(false);
    // add revolute joint between the base and ground
    auto base_rev = chrono_types::make_shared<ChLinkLockRevolute>();
    base_rev->Initialize(base, ground, ChCoordsys<>(ChVector<>(0.0, 0.0, -9.0), revoluteRot));
    system.AddLink(base_rev);

    // ---------------- DT3 set up (flap decay, base fixed, no waves) ---------------------------------
    // base->SetPos(ChVector<>(0.0, 0.0, -9.0));
    // double fore_ang_rad = CH_C_PI / 18.0; // fore flap starts with 10 degree initial rotation
    // flapFore->SetRot(Q_from_AngAxis(fore_ang_rad, VECT_Y));
    // flapFore->SetPos(ChVector<>(-12.5 + 3.5 * std::cos(CH_C_PI / 2.0 - fore_ang_rad),
    //	0.0,
    //	-9.0 + 3.5 * std::sin(CH_C_PI / 2.0 - fore_ang_rad)));
    // double aft_ang_rad = 0.0;
    // flapAft->SetRot(Q_from_AngAxis(aft_ang_rad, VECT_Y));
    // flapAft->SetPos(ChVector<>(12.5 + 3.5 * std::cos(CH_C_PI / 2.0 - aft_ang_rad),
    //	0.0,
    //	-9.0 + 3.5 * std::sin(CH_C_PI / 2.0 - fore_ang_rad)));
    //// set up revolute joints with damping for each flap
    // auto revoluteFore = chrono_types::make_shared<ChLinkLockRevolute>();
    // auto revoluteAft = chrono_types::make_shared<ChLinkLockRevolute>();
    // ChQuaternion<> revoluteRot = Q_from_AngX(CH_C_PI / 2.0); // do not change
    // revoluteFore->Initialize(base, flapFore, ChCoordsys<>(ChVector<>(-12.5, 0.0, -9.0), revoluteRot));
    // system.AddLink(revoluteFore);
    // revoluteAft->Initialize(base, flapAft, ChCoordsys<>(ChVector<>(12.5, 0.0, -9.0), revoluteRot));
    // system.AddLink(revoluteAft);
    //// create ground
    // auto ground = chrono_types::make_shared<ChBody>();
    // system.AddBody(ground);
    // ground->SetPos(ChVector<>(0, 0, -12.0));
    // ground->SetIdentifier(-1);
    // ground->SetBodyFixed(true);
    // ground->SetCollide(false);
    //// fix base to ground with special constraint (don't use setfixed() because of mass matrix)
    // auto anchor = chrono_types::make_shared<ChLinkMateGeneric>();
    // anchor->Initialize(base, ground, false, base->GetVisualModelFrame(), base->GetVisualModelFrame());
    // system.Add(anchor);
    // anchor->SetConstrainedCoords(true, true, true, true, true, true);  // x, y, z, Rx, Ry, Rz

    // ---------------- End DT specific set up, now add hydro forces ----------------------------------

    // define wave parameters (not used in this demo TODO have hydroforces constructor without hydro inputs)
    HydroInputs my_hydro_inputs;
    my_hydro_inputs.mode = WaveMode::noWaveCIC;

    // set up hydro forces
    std::vector<std::shared_ptr<ChBody>> bodies;
    bodies.push_back(base);
    bodies.push_back(flapFore);
    bodies.push_back(flapAft);
    TestHydro hydroforces(bodies, h5fname, my_hydro_inputs);

    // for profiling
    auto start = std::chrono::high_resolution_clock::now();

    // main simulation loop
    ui.Init(&system, "F3OF - Decay Test");
    ui.SetCamera(0, -50, -10, 0, 0, -10);

    while (system.GetChTime() <= simulationDuration) {
        if (ui.IsRunning(timestep) == false) break;

        if (ui.simulationStarted) {
            system.DoStepDynamics(timestep);

            // append data to output vector
            time_vector.push_back(system.GetChTime());
            base_surge.push_back(base->GetPos().x());
            base_pitch.push_back(base->GetRot().Q_to_Euler123().y());
            fore_pitch.push_back(flapFore->GetRot().Q_to_Euler123().y());
            aft_pitch.push_back(flapAft->GetRot().Q_to_Euler123().y());
        }
    }

    if (saveDataOn) {
        std::ofstream outputFile;
        outputFile.open("./results/f3of/decay/f3of_decay.txt");
        if (!outputFile.is_open()) {
            if (!std::filesystem::exists("./results/f3of/decay")) {
                std::cout << "Path " << std::filesystem::absolute("./results/f3of/decay")
                          << " does not exist, creating it now..." << std::endl;
                std::filesystem::create_directory("./results");
                std::filesystem::create_directory("./results/f3of");
                std::filesystem::create_directory("./results/f3of/decay");
                outputFile.open("./results/f3of/decay/f3of_decay.txt");
                if (!outputFile.is_open()) {
                    std::cout << "Still cannot open file, ending program" << std::endl;
                    return 0;
                }
            }
        }
        outputFile << std::left << std::setw(10) << "Time (s)" << std::right << std::setw(16) << "Base Surge (m)"
                   << std::right << std::setw(16) << "Base Pitch (radians)" << std::right << std::setw(16)
                   << "Flap Fore Pitch (radians)" << std::right << std::setw(16) << "Flap Aft Pitch (radians)"
                   << std::endl;
        for (int i = 0; i < time_vector.size(); ++i)
            outputFile << std::left << std::setw(10) << std::setprecision(2) << std::fixed << time_vector[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << base_surge[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << base_pitch[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << fore_pitch[i]
                       << std::right << std::setw(16) << std::setprecision(4) << std::fixed << aft_pitch[i]
                       << std::endl;
        outputFile.close();
    }

    std::cout << "Simulation finished." << std::endl;
    return 0;
}