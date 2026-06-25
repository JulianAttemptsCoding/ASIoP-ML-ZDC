void inspect_root_out() {
    TFile *f = TFile::Open("plots/energy_reconstruction.root");
    if (!f||f->IsZombie()) { printf("Cannot open\n"); return; }
    printf("Keys in energy_reconstruction.root:\n");
    f->ls();
    f->Close();
}
