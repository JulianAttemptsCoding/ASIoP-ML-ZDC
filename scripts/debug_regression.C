// debug_regression.C — inspect one raw file and verify the regression branches are readable.
// Usage:
//   root -l -q 'scripts/debug_regression.C("data/gamma/outfile_gamma1GeV.root")'

void debug_regression(const char* path="data/gamma/outfile_gamma1GeV.root") {
    TFile* f = TFile::Open(path, "READ");
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open %s\n", path); return; }
    TTree* t = (TTree*)f->Get("events");
    if (!t) { printf("[ERROR] Missing events tree\n"); f->Close(); return; }

    TLeaf* lEcal = t->GetLeaf("EcalFarForwardZDCHits.energy");
    TLeaf* lHcal = t->GetLeaf("HcalFarForwardZDCHits.energy");
    TLeaf* lPx   = t->GetLeaf("MCParticles.momentum.x");
    TLeaf* lPy   = t->GetLeaf("MCParticles.momentum.y");
    TLeaf* lPz   = t->GetLeaf("MCParticles.momentum.z");
    TLeaf* lMass = t->GetLeaf("MCParticles.mass");
    TLeaf* lStat = t->GetLeaf("MCParticles.generatorStatus");
    if (!lEcal || !lHcal || !lPx || !lPy || !lPz || !lMass) {
        printf("[ERROR] Missing required leaves. Use t->Print() to inspect branch names.\n");
        t->Print(); f->Close(); return;
    }

    printf("=== First 10 events: %s ===\n", path);
    for (Long64_t iev = 0; iev < TMath::Min((Long64_t)10, t->GetEntries()); ++iev) {
        t->GetEntry(iev);
        double ecal = 0, hcal = 0;
        for (int i=0; i<lEcal->GetLen(); ++i) ecal += lEcal->GetValue(i);
        for (int i=0; i<lHcal->GetLen(); ++i) hcal += lHcal->GetValue(i);
        int ip = 0;
        if (lStat) for (int j=0; j<lStat->GetLen(); ++j) if ((int)lStat->GetValue(j)==1) { ip=j; break; }
        double beam = sqrt(pow(lPx->GetValue(ip),2)+pow(lPy->GetValue(ip),2)+pow(lPz->GetValue(ip),2)+pow(lMass->GetValue(ip),2));
        printf("event %lld: nEcal=%d ECal=%g  nHcal=%d HCal=%g  Ebeam=%g\n", iev, lEcal->GetLen(), ecal, lHcal->GetLen(), hcal, beam);
    }
    f->Close();
}
