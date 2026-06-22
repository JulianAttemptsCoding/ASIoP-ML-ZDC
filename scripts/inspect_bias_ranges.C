// inspect_bias_ranges.C — print gBias_dcb points for all neutron + gamma m/w combos
void inspect_bias_ranges() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";
    const char* ptypes[] = {"neutron","gamma"};
    const char* dirs[]   = {"20260324_neutron_LYSO_diffAngle","20260421_gamma_LYSO_diffAngle"};
    int mvals[] = {0,1,2};
    int wvals[] = {0,1};

    for (int pt = 0; pt < 2; pt++) {
        printf("\n=== %s gBias_dcb ===\n", ptypes[pt]);
        for (int m : mvals) for (int w : wvals) {
            TString path = TString::Format("%s/%s/res2_1_m%d_w%d_%s.root",
                                           base, dirs[pt], m, w, ptypes[pt]);
            TFile *f = TFile::Open(path);
            if (!f || f->IsZombie()) { printf("  m%d_w%d: [cannot open]\n",m,w); continue; }
            TGraphErrors *g = (TGraphErrors*)f->Get("gBias_dcb");
            if (!g) { printf("  m%d_w%d: [no gBias_dcb]\n",m,w); f->Close(); continue; }
            printf("  m%d_w%d: %d pts  ", m, w, g->GetN());
            for (int i = 0; i < g->GetN(); i++)
                printf("[E=%.0f,mu=%.4f] ", g->GetX()[i], g->GetY()[i]);
            printf("\n");
            f->Close();
        }
    }
    printf("Done.\n");
}
