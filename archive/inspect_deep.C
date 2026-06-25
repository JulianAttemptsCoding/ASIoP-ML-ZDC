// inspect_deep.C — inspects canvas primitives and subdirectories
void inspect_deep() {
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // --- res2_1 file: what's inside each canvas ---
    TString f1path = TString::Format("%s/20260324_neutron_LYSO_diffAngle/res2_1_m0_w0_neutron.root", base);
    TFile *f1 = TFile::Open(f1path);
    printf("\n=== res2_1_m0_w0_neutron.root CANVAS PRIMITIVES ===\n");
    const char* canvases[] = {"c_Eorg","c_Erec","c_Erec_dcb","c_Erec_res","c_Erec_bias"};
    for (int i = 0; i < 5; i++) {
        TCanvas *c = (TCanvas*)f1->Get(canvases[i]);
        if (!c) { printf("  [MISSING] %s\n", canvases[i]); continue; }
        printf("\n  Canvas: %s\n", canvases[i]);
        TList *prims = c->GetListOfPrimitives();
        TIter it(prims);
        TObject *obj;
        while ((obj = it())) {
            printf("    prim: %-30s  class: %s\n", obj->GetName(), obj->ClassName());
            // If pad, go one level deeper
            if (obj->InheritsFrom("TPad")) {
                TPad *pad = (TPad*)obj;
                TIter it2(pad->GetListOfPrimitives());
                TObject *o2;
                while ((o2 = it2())) {
                    printf("      sub-prim: %-28s  class: %s\n", o2->GetName(), o2->ClassName());
                }
            }
        }
    }
    // gReso_dcb points
    TGraphErrors *gr = (TGraphErrors*)f1->Get("gReso_dcb");
    if (gr) {
        printf("\n  gReso_dcb points (x=energy GeV, y=resolution):\n");
        for (int i = 0; i < gr->GetN(); i++)
            printf("    [%d] E=%.1f  reso=%.4f  err=%.4f\n", i, gr->GetX()[i], gr->GetY()[i], gr->GetEY()[i]);
    }
    f1->Close();

    // --- Comparison_Results.root: canvas primitives ---
    TString f2path = TString::Format("%s/20260324_neutron_LYSO_diffAngle/Comparison_Results.root", base);
    TFile *f2 = TFile::Open(f2path);
    printf("\n=== neutron Comparison_Results.root CANVAS PRIMITIVES ===\n");
    const char* crs[] = {"canv_grX_sigma","canv_grX_mean","canv_grY_sigma","canv_grY_mean"};
    for (int i = 0; i < 4; i++) {
        TCanvas *c = (TCanvas*)f2->Get(crs[i]);
        if (!c) continue;
        printf("\n  Canvas: %s\n", crs[i]);
        TList *prims = c->GetListOfPrimitives();
        TIter it(prims);
        TObject *obj;
        while ((obj = it())) {
            printf("    prim: %-30s  class: %s\n", obj->GetName(), obj->ClassName());
            if (obj->InheritsFrom("TPad")) {
                TPad *pad = (TPad*)obj;
                TIter it2(pad->GetListOfPrimitives());
                TObject *o2;
                while ((o2 = it2())) {
                    printf("      sub: %-30s  class: %s\n", o2->GetName(), o2->ClassName());
                }
            }
        }
    }
    f2->Close();

    // --- res3_2_neutron7.root subdirectories ---
    TString f3path = TString::Format("%s/20260324_neutron_LYSO_diffAngle/res3_2_neutron7.root", base);
    TFile *f3 = TFile::Open(f3path);
    printf("\n=== res3_2_neutron7.root SUBDIRECTORIES ===\n");
    TDirectoryFile *de = (TDirectoryFile*)f3->Get("dir_tr_ecal");
    TDirectoryFile *dh = (TDirectoryFile*)f3->Get("dir_tr_hcal");
    if (de) { printf("  dir_tr_ecal:\n"); de->ls(); }
    if (dh) { printf("  dir_tr_hcal:\n"); dh->ls(); }
    f3->Close();

    printf("\nDone.\n");
}
