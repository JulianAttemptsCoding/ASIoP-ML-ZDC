// inspect_raw.C — print branch/leaf structure of one raw PODIO file.
// Usage: root -l -q 'scripts/inspect_raw.C("data/gamma/outfile_gamma1GeV.root")'
void inspect_raw(const char* path="data/gamma/outfile_gamma1GeV.root") {
    TFile* f = TFile::Open(path, "READ");
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open %s\n", path); return; }
    f->ls();
    TTree* t = (TTree*)f->Get("events");
    if (t) { t->Print(); t->Scan("*", "", "", 5); }
    else printf("[ERROR] Missing events tree\n");
    f->Close();
}
