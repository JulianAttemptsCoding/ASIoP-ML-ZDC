// inspect_files.C
// Lists all ROOT objects in key data files — run once to learn structure
// Usage: root -b -q inspect_files.C

void inspect_files() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    const char* files[] = {
        "20260324_neutron_LYSO_diffAngle/res2_1_m0_w0_neutron.root",
        "20260421_gamma_LYSO_diffAngle/res2_1_m0_w0_gamma.root",
        "20260324_neutron_LYSO_diffAngle/Comparison_Results.root",
        "20260421_gamma_LYSO_diffAngle/Comparison_Results.root",
        "20260324_neutron_LYSO_diffAngle/res3_2_neutron7.root",
        "20260324_neutron_LYSO_diffAngle/res3_2_fitLayerPM35_neutron7.root"
    };

    int n = sizeof(files)/sizeof(files[0]);
    for (int i = 0; i < n; i++) {
        TString path = TString::Format("%s/%s", base, files[i]);
        TFile *f = TFile::Open(path);
        if (!f || f->IsZombie()) {
            printf("\n[ERROR] Cannot open: %s\n", path.Data());
            continue;
        }
        printf("\n========== %s ==========\n", files[i]);
        f->ls();

        // For TTree objects, also print branch names
        TIter next(f->GetListOfKeys());
        TKey *key;
        while ((key = (TKey*)next())) {
            TString cls = key->GetClassName();
            if (cls == "TTree" || cls == "TNtuple") {
                TTree *t = (TTree*)f->Get(key->GetName());
                printf("  [TTree branches: %s]\n", key->GetName());
                t->GetListOfBranches()->ls();
            }
            // For TObjArray, print contents
            if (cls == "TObjArray") {
                TObjArray *arr = (TObjArray*)f->Get(key->GetName());
                printf("  [TObjArray size=%d: %s]\n", arr->GetSize(), key->GetName());
                for (int j = 0; j < arr->GetSize(); j++) {
                    TObject *obj = arr->At(j);
                    if (obj) printf("    [%d] %s : %s\n", j, obj->GetName(), obj->ClassName());
                }
            }
        }
        f->Close();
    }
    printf("\nDone.\n");
}
