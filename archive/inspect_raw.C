// inspect_raw.C — print TTree branch structure of raw outfile_*.root
void inspect_raw() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    const char* files[] = {
        "20260324_neutron_LYSO_diffAngle/outfile_neutron1.root",
        "20260421_gamma_LYSO_diffAngle/outfile_gamma1.root",
    };

    for (int fi = 0; fi < 2; fi++) {
        TString path = TString::Format("%s/%s", base, files[fi]);
        TFile *f = TFile::Open(path);
        if (!f || f->IsZombie()) { printf("[ERROR] %s\n", path.Data()); continue; }
        printf("\n====== %s ======\n", files[fi]);
        f->ls();

        // Find all TTrees/TNtuples and print branches
        TIter next(f->GetListOfKeys());
        TKey *key;
        while ((key = (TKey*)next())) {
            TString cls = key->GetClassName();
            if (cls.Contains("TTree") || cls.Contains("TNtuple")) {
                TTree *t = (TTree*)f->Get(key->GetName());
                printf("\n  TTree: %s  entries=%lld\n", t->GetName(), t->GetEntries());
                TObjArray *branches = t->GetListOfBranches();
                for (int i = 0; i < branches->GetEntries(); i++) {
                    TBranch *b = (TBranch*)branches->At(i);
                    printf("    branch[%d]: %-30s  title: %s\n",
                           i, b->GetName(), b->GetTitle());
                }
                // Print first 5 entries to see values
                printf("\n  First 5 entries:\n");
                t->Scan("*", "", "", 5);
            }
        }
        f->Close();
    }
    printf("\nDone.\n");
}
