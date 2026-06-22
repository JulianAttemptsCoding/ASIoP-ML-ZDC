// inspect_output_root.C — list all contents of energy_reconstruction.root
void inspect_output_root() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";
    TFile *f = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base));
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open output ROOT file\n"); return; }

    printf("=== Contents of plots/energy_reconstruction.root ===\n");
    f->ls();

    printf("\n=== Canvas dimensions check ===\n");
    TIter next(f->GetListOfKeys());
    TKey *key;
    while ((key = (TKey*)next())) {
        TCanvas *c = (TCanvas*)f->Get(key->GetName());
        if (c && c->InheritsFrom("TCanvas")) {
            printf("  Canvas: %-35s  size: %d x %d\n",
                   key->GetName(), c->GetWw(), c->GetWh());
            // Count pads
            int npads = 0;
            TIter itp(c->GetListOfPrimitives());
            TObject *o;
            while ((o = itp())) if (o->InheritsFrom("TPad")) npads++;
            printf("          pads: %d\n", npads);
        }
    }
    f->Close();
    printf("Done.\n");
}
