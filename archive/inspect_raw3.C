// inspect_raw3.C — find gamma outfiles, confirm neutron energy-file mapping
void inspect_raw3() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // List all files in gamma directory
    printf("=== gamma dir contents ===\n");
    gSystem->Exec(TString::Format("ls '%s/20260421_gamma_LYSO_diffAngle/'", base));

    printf("\n=== neutron dir contents ===\n");
    gSystem->Exec(TString::Format("ls '%s/20260324_neutron_LYSO_diffAngle/'", base));

    // Confirm neutron file -> beam energy mapping from MCParticles momentum
    printf("\n=== neutron file -> beam energy (from MCParticles momentum pz) ===\n");
    const char* nfiles[] = {"outfile_neutron1.root","outfile_neutron2.root","outfile_neutron3.root",
                            "outfile_neutron5.root","outfile_neutron6.root","outfile_neutron7.root"};
    for (auto fn : nfiles) {
        TString p = TString::Format("%s/20260324_neutron_LYSO_diffAngle/%s", base, fn);
        TFile *f = TFile::Open(p);
        if (!f||f->IsZombie()) continue;
        TTree *t = (TTree*)f->Get("events");
        // Read first event MCParticles primary particle (generatorStatus==1)
        Float_t px, py, pz, mass_f;
        Double_t mass;
        Int_t pdg, genStat, npart;
        t->SetBranchAddress("MCParticles_",           &npart);
        t->SetBranchAddress("MCParticles.momentum.x", &px);
        t->SetBranchAddress("MCParticles.momentum.y", &py);
        t->SetBranchAddress("MCParticles.momentum.z", &pz);
        t->SetBranchAddress("MCParticles.mass",       &mass);
        t->SetBranchAddress("MCParticles.PDG",        &pdg);
        t->GetEntry(0);
        double E = sqrt((double)px*px + (double)py*py + (double)pz*pz + mass*mass);
        printf("  %s  PDG=%d  pz=%.3f GeV/c  E=%.2f GeV\n", fn, pdg, (double)pz, E);
        f->Close();
    }

    // Try to find gamma outfiles with any name pattern
    printf("\n=== searching gamma dir for any *.root outfiles ===\n");
    gSystem->Exec(TString::Format("ls '%s/20260421_gamma_LYSO_diffAngle/'*.root 2>/dev/null | head -30", base));

    printf("\nDone.\n");
}
