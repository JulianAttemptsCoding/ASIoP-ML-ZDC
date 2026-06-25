// debug_regression.C — verify WLS regression works on 1 GeV gamma
void debug_regression() {
    const char* path = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC"
                       "/20260421_gamma_LYSO_diffAngle/outfile_gamma1GeV.root";
    TFile *f = TFile::Open(path);
    TTree *t = (TTree*)f->Get("events");

    TLeaf *lEcal = t->GetLeaf("EcalFarForwardZDCHits.energy");
    TLeaf *lHcal = t->GetLeaf("HcalFarForwardZDCHits.energy");
    TLeaf *lPz   = t->GetLeaf("MCParticles.momentum.z");
    TLeaf *lMass = t->GetLeaf("MCParticles.mass");

    // Print first 5 events raw
    printf("=== First 5 events raw ===\n");
    for (int iev = 0; iev < 5; iev++) {
        t->GetEntry(iev);
        double sumE=0, sumH=0;
        for (int ih=0;ih<lEcal->GetLen();ih++) sumE+=lEcal->GetValue(ih);
        for (int ih=0;ih<lHcal->GetLen();ih++) sumH+=lHcal->GetValue(ih);
        double pz=lPz->GetValue(0), m=lMass->GetValue(0);
        double eb=sqrt(pz*pz+m*m);
        printf("  ev%d: nEcal=%d E_ECAL=%.4f  nHcal=%d E_HCAL=%.6f  E_beam=%.4f\n",
               iev, lEcal->GetLen(), sumE, lHcal->GetLen(), sumH, eb);
    }

    // Collect all events
    std::vector<double> ve, vh, vb;
    for (Long64_t iev=0; iev<t->GetEntries(); iev++) {
        t->GetEntry(iev);
        double sumE=0, sumH=0;
        for (int ih=0;ih<lEcal->GetLen();ih++) sumE+=lEcal->GetValue(ih);
        for (int ih=0;ih<lHcal->GetLen();ih++) sumH+=lHcal->GetValue(ih);
        double pz=lPz->GetValue(0), m=lMass->GetValue(0);
        double eb=sqrt(pz*pz+m*m);
        if (eb<1e-6) continue;
        ve.push_back(sumE); vh.push_back(sumH); vb.push_back(eb);
    }
    printf("\nGot %d valid events\n", (int)ve.size());
    f->Close();

    // Build 3x3 WLS normal equations for m0 w1
    TMatrixD AtWA(3,3); TVectorD AtWb(3);
    for (int i=0;i<(int)ve.size();i++) {
        double w=sqrt(vb[i]), row[3]={1,ve[i],vh[i]};
        for (int j=0;j<3;j++) {
            AtWb(j) += w*row[j]*vb[i];
            for (int l=0;l<3;l++) AtWA(j,l) += w*row[j]*row[l];
        }
    }
    printf("\nAtWA:\n");
    AtWA.Print();
    printf("AtWb: %.6f  %.6f  %.6f\n", AtWb(0), AtWb(1), AtWb(2));

    // Method 1: TDecompSVD
    printf("\n--- TDecompSVD ---\n");
    TDecompSVD svd(AtWA);
    Bool_t dok = svd.Decompose();
    printf("Decompose ok=%d\n", (int)dok);
    TVectorD pSVD = AtWb;
    Bool_t sok = svd.Solve(pSVD);
    printf("Solve ok=%d  p=[%.6f  %.6f  %.6f]\n", (int)sok, pSVD(0), pSVD(1), pSVD(2));

    // Method 2: TMatrixD::Invert
    printf("\n--- TMatrixD::Invert ---\n");
    TMatrixD AtWA2 = AtWA;
    Double_t det;
    AtWA2.Invert(&det);
    printf("det=%.6e\n", det);
    TVectorD pINV(3);
    for (int j=0;j<3;j++) for (int l=0;l<3;l++) pINV(j)+=AtWA2(j,l)*AtWb(l);
    printf("p=[%.6f  %.6f  %.6f]\n", pINV(0), pINV(1), pINV(2));

    // Method 3: TLinearFitter
    printf("\n--- TLinearFitter ---\n");
    TLinearFitter lf(2, "1 ++ x[0] ++ x[1]");
    for (int i=0;i<(int)ve.size();i++) {
        double xx[2]={ve[i],vh[i]};
        double err = 1.0/sqrt(sqrt(vb[i]));  // w1: err=1/sqrt(sqrt(E))
        lf.AddPoint(xx, vb[i], err);
    }
    lf.Eval();
    printf("p=[%.6f  %.6f  %.6f]\n",
           lf.GetParameter(0), lf.GetParameter(1), lf.GetParameter(2));

    // Verify: apply p to first event
    printf("\n=== Verify on first event ===\n");
    double p0=lf.GetParameter(0), p1=lf.GetParameter(1), p2=lf.GetParameter(2);
    printf("p0=%.4f  p1=%.4f  p2=%.4f\n", p0, p1, p2);
    if (ve.size()>0) {
        double erec = p0 + p1*ve[0] + p2*vh[0];
        printf("E_beam=%.4f  E_ECAL=%.4f  E_HCAL=%.6f  E_rec=%.4f  ratio=%.4f\n",
               vb[0], ve[0], vh[0], erec, erec/vb[0]);
    }
}
