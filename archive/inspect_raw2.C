// inspect_raw2.C — deep PODIO leaf inspection + list all raw files
void inspect_raw2() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // Check what neutron raw files exist and how many entries each has
    printf("=== neutron raw files ===\n");
    const char* nfiles[] = {"outfile_neutron1.root","outfile_neutron2.root","outfile_neutron3.root",
                            "outfile_neutron5.root","outfile_neutron6.root","outfile_neutron7.root"};
    for (auto fn : nfiles) {
        TString p = TString::Format("%s/20260324_neutron_LYSO_diffAngle/%s", base, fn);
        TFile *f = TFile::Open(p);
        if (!f||f->IsZombie()) { printf("  [missing] %s\n",fn); continue; }
        TTree *t = (TTree*)f->Get("events");
        printf("  %s  entries=%lld\n", fn, t ? t->GetEntries() : -1);
        f->Close();
    }
    printf("\n=== gamma raw files ===\n");
    const char* gfiles[] = {"outfile_gamma1.root","outfile_gamma2.root","outfile_gamma3.root",
                            "outfile_gamma4.root","outfile_gamma5.root","outfile_gamma6.root",
                            "outfile_gamma7.root","outfile_gamma8.root"};
    for (auto fn : gfiles) {
        TString p = TString::Format("%s/20260421_gamma_LYSO_diffAngle/%s", base, fn);
        TFile *f = TFile::Open(p);
        if (!f||f->IsZombie()) { printf("  [missing] %s\n",fn); continue; }
        TTree *t = (TTree*)f->Get("events");
        printf("  %s  entries=%lld\n", fn, t ? t->GetEntries() : -1);
        f->Close();
    }

    // Deep leaf inspection on neutron1
    printf("\n=== Deep leaf names: neutron1 'events' tree ===\n");
    TFile *f = TFile::Open(TString::Format("%s/20260324_neutron_LYSO_diffAngle/outfile_neutron1.root",base));
    TTree *t = (TTree*)f->Get("events");

    // Print all leaves
    TObjArray *leaves = t->GetListOfLeaves();
    printf("  Total leaves: %d\n", leaves->GetEntries());
    for (int i = 0; i < leaves->GetEntries(); i++) {
        TLeaf *lf = (TLeaf*)leaves->At(i);
        printf("  leaf[%3d]: %-60s  type: %s\n", i, lf->GetName(), lf->GetTypeName());
    }

    // Print a few MCParticles values to find beam energy
    printf("\n=== MCParticles leaves with values (first event) ===\n");
    for (int i = 0; i < leaves->GetEntries(); i++) {
        TLeaf *lf = (TLeaf*)leaves->At(i);
        TString nm = lf->GetName();
        if (nm.Contains("MCParticle") && !nm.Contains("parents") && !nm.Contains("daughters")) {
            t->GetEntry(0);
            printf("  %-55s = %g\n", lf->GetName(), lf->GetValue(0));
        }
    }
    f->Close();
    printf("\nDone.\n");
}
