// inspect_output_root.C — list canvases/graphs in the generated TBrowser output.
void inspect_output_root(const char* path="plots/energy_reconstruction_browser.root") {
    TFile* f = TFile::Open(path, "READ");
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open %s\n", path); return; }
    f->ls("-m");
    printf("\nOpen interactively with:\n  root -l %s\n  new TBrowser\n", path);
    f->Close();
}
