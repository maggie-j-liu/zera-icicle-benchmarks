; ModuleID = '/home/magpie/zera-icicle-benchmarks/zera_vector_add_profile_int.cpp'
source_filename = "/home/magpie/zera-icicle-benchmarks/zera_vector_add_profile_int.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

module asm ".globl _ZSt21ios_base_library_initv"

%"class.std::basic_ostream" = type { ptr, %"class.std::basic_ios" }
%"class.std::basic_ios" = type { %"class.std::ios_base", ptr, i8, i8, ptr, ptr, ptr, ptr }
%"class.std::ios_base" = type { ptr, i64, i64, i32, i32, i32, ptr, %"struct.std::ios_base::_Words", [8 x %"struct.std::ios_base::_Words"], i32, ptr, %"class.std::locale" }
%"struct.std::ios_base::_Words" = type { ptr, i64 }
%"class.std::locale" = type { ptr }
%"class.std::mersenne_twister_engine" = type { [312 x i64], i64 }

$_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv = comdat any

@_ZSt4cout = external global %"class.std::basic_ostream", align 8
@.str = private unnamed_addr constant [28 x i8] c"\0A=== Zera vector_add: size=\00", align 1
@.str.1 = private unnamed_addr constant [5 x i8] c" ===\00", align 1

; Function Attrs: mustprogress nounwind memory(argmem: readwrite) uwtable
define void @_Z10vector_addPKmS0_Pmm(ptr nocapture noundef readonly %a, ptr nocapture noundef readonly %b, ptr nocapture noundef writeonly %out, i64 noundef %size) local_unnamed_addr #0 {
entry:
  %b25 = ptrtoint ptr %b to i64
  %a23 = ptrtoint ptr %a to i64
  %out22 = ptrtoint ptr %out to i64
  %syncreg = tail call token @llvm.syncregion.start()
  %cmp.not = icmp eq i64 %size, 0
  br i1 %cmp.not, label %cleanup, label %pfor.cond.preheader

pfor.cond.preheader:                              ; preds = %entry
  %xtraiter = and i64 %size, 2047
  %0 = icmp ult i64 %size, 2048
  br i1 %0, label %pfor.cond.cleanup.strpm-lcssa, label %pfor.cond.preheader.new

pfor.cond.preheader.new:                          ; preds = %pfor.cond.preheader
  %stripiter14 = lshr i64 %size, 11
  %1 = sub i64 %out22, %a23
  %2 = sub i64 %out22, %b25
  %diff.check = icmp ult i64 %1, 32
  %diff.check18 = icmp ult i64 %2, 32
  %conflict.rdx = or i1 %diff.check, %diff.check18
  br label %pfor.cond.strpm.outer

pfor.cond.strpm.outer:                            ; preds = %pfor.inc.strpm.outer, %pfor.cond.preheader.new
  %niter = phi i64 [ 0, %pfor.cond.preheader.new ], [ %niter.nadd, %pfor.inc.strpm.outer ]
  detach within %syncreg, label %pfor.body.entry.strpm.outer, label %pfor.inc.strpm.outer

pfor.body.entry.strpm.outer:                      ; preds = %pfor.cond.strpm.outer
  %3 = shl i64 %niter, 11
  br i1 %conflict.rdx, label %pfor.cond, label %vector.body

vector.body:                                      ; preds = %pfor.body.entry.strpm.outer, %vector.body
  %index = phi i64 [ %index.next, %vector.body ], [ 0, %pfor.body.entry.strpm.outer ]
  %offset.idx = add i64 %3, %index
  %4 = getelementptr inbounds i64, ptr %a, i64 %offset.idx
  %wide.load = load <2 x i64>, ptr %4, align 8, !tbaa !4
  %5 = getelementptr inbounds i64, ptr %b, i64 %offset.idx
  %wide.load20 = load <2 x i64>, ptr %5, align 8, !tbaa !4
  %6 = add <2 x i64> %wide.load20, %wide.load
  %7 = getelementptr inbounds i64, ptr %out, i64 %offset.idx
  store <2 x i64> %6, ptr %7, align 8, !tbaa !4
  %index.next = add nuw i64 %index, 2
  %8 = icmp eq i64 %index.next, 2048
  br i1 %8, label %pfor.inc.reattach, label %vector.body, !llvm.loop !8

pfor.cond:                                        ; preds = %pfor.body.entry.strpm.outer, %pfor.cond
  %__begin.0 = phi i64 [ %inc, %pfor.cond ], [ %3, %pfor.body.entry.strpm.outer ]
  %inneriter = phi i64 [ %inneriter.nsub, %pfor.cond ], [ 2048, %pfor.body.entry.strpm.outer ]
  %arrayidx = getelementptr inbounds i64, ptr %a, i64 %__begin.0
  %9 = load i64, ptr %arrayidx, align 8, !tbaa !4
  %arrayidx1 = getelementptr inbounds i64, ptr %b, i64 %__begin.0
  %10 = load i64, ptr %arrayidx1, align 8, !tbaa !4
  %add2 = add i64 %10, %9
  %arrayidx3 = getelementptr inbounds i64, ptr %out, i64 %__begin.0
  store i64 %add2, ptr %arrayidx3, align 8, !tbaa !4
  %inc = add nuw i64 %__begin.0, 1
  %inneriter.nsub = add nsw i64 %inneriter, -1
  %inneriter.ncmp = icmp eq i64 %inneriter.nsub, 0
  br i1 %inneriter.ncmp, label %pfor.inc.reattach, label %pfor.cond, !llvm.loop !15

pfor.inc.reattach:                                ; preds = %vector.body, %pfor.cond
  reattach within %syncreg, label %pfor.inc.strpm.outer

pfor.inc.strpm.outer:                             ; preds = %pfor.inc.reattach, %pfor.cond.strpm.outer
  %niter.nadd = add nuw i64 %niter, 1
  %niter.ncmp = icmp eq i64 %niter.nadd, %stripiter14
  br i1 %niter.ncmp, label %pfor.cond.cleanup.strpm-lcssa, label %pfor.cond.strpm.outer, !llvm.loop !16

pfor.cond.cleanup.strpm-lcssa:                    ; preds = %pfor.inc.strpm.outer, %pfor.cond.preheader
  %lcmp.mod.not = icmp eq i64 %xtraiter, 0
  br i1 %lcmp.mod.not, label %pfor.cond.cleanup, label %pfor.cond.epil.preheader

pfor.cond.epil.preheader:                         ; preds = %pfor.cond.cleanup.strpm-lcssa
  %11 = and i64 %size, -2048
  %min.iters.check = icmp ult i64 %xtraiter, 8
  br i1 %min.iters.check, label %pfor.cond.epil.preheader41, label %vector.memcheck21

vector.memcheck21:                                ; preds = %pfor.cond.epil.preheader
  %12 = sub i64 %out22, %a23
  %diff.check24 = icmp ult i64 %12, 32
  %13 = sub i64 %out22, %b25
  %diff.check26 = icmp ult i64 %13, 32
  %conflict.rdx27 = or i1 %diff.check24, %diff.check26
  br i1 %conflict.rdx27, label %pfor.cond.epil.preheader41, label %vector.ph30

vector.ph30:                                      ; preds = %vector.memcheck21
  %n.mod.vf = and i64 %size, 1
  %n.vec = sub nsw i64 %xtraiter, %n.mod.vf
  %ind.end31 = add i64 %11, %n.vec
  br label %vector.body35

vector.body35:                                    ; preds = %vector.body35, %vector.ph30
  %index36 = phi i64 [ 0, %vector.ph30 ], [ %index.next40, %vector.body35 ]
  %offset.idx37 = add i64 %11, %index36
  %14 = getelementptr inbounds i64, ptr %a, i64 %offset.idx37
  %wide.load38 = load <2 x i64>, ptr %14, align 8, !tbaa !4
  %15 = getelementptr inbounds i64, ptr %b, i64 %offset.idx37
  %wide.load39 = load <2 x i64>, ptr %15, align 8, !tbaa !4
  %16 = add <2 x i64> %wide.load39, %wide.load38
  %17 = getelementptr inbounds i64, ptr %out, i64 %offset.idx37
  store <2 x i64> %16, ptr %17, align 8, !tbaa !4
  %index.next40 = add nuw i64 %index36, 2
  %18 = icmp eq i64 %index.next40, %n.vec
  br i1 %18, label %middle.block28, label %vector.body35, !llvm.loop !19

middle.block28:                                   ; preds = %vector.body35
  %cmp.n = icmp eq i64 %n.mod.vf, 0
  br i1 %cmp.n, label %pfor.cond.cleanup, label %pfor.cond.epil.preheader41

pfor.cond.epil.preheader41:                       ; preds = %middle.block28, %vector.memcheck21, %pfor.cond.epil.preheader
  %__begin.0.epil.ph = phi i64 [ %11, %vector.memcheck21 ], [ %11, %pfor.cond.epil.preheader ], [ %ind.end31, %middle.block28 ]
  %epil.iter.ph = phi i64 [ %xtraiter, %vector.memcheck21 ], [ %xtraiter, %pfor.cond.epil.preheader ], [ %n.mod.vf, %middle.block28 ]
  br label %pfor.cond.epil

pfor.cond.epil:                                   ; preds = %pfor.cond.epil.preheader41, %pfor.cond.epil
  %__begin.0.epil = phi i64 [ %inc.epil, %pfor.cond.epil ], [ %__begin.0.epil.ph, %pfor.cond.epil.preheader41 ]
  %epil.iter = phi i64 [ %epil.iter.sub, %pfor.cond.epil ], [ %epil.iter.ph, %pfor.cond.epil.preheader41 ]
  %arrayidx.epil = getelementptr inbounds i64, ptr %a, i64 %__begin.0.epil
  %19 = load i64, ptr %arrayidx.epil, align 8, !tbaa !4
  %arrayidx1.epil = getelementptr inbounds i64, ptr %b, i64 %__begin.0.epil
  %20 = load i64, ptr %arrayidx1.epil, align 8, !tbaa !4
  %add2.epil = add i64 %20, %19
  %arrayidx3.epil = getelementptr inbounds i64, ptr %out, i64 %__begin.0.epil
  store i64 %add2.epil, ptr %arrayidx3.epil, align 8, !tbaa !4
  %inc.epil = add nuw nsw i64 %__begin.0.epil, 1
  %epil.iter.sub = add nsw i64 %epil.iter, -1
  %epil.iter.cmp.not = icmp eq i64 %epil.iter.sub, 0
  br i1 %epil.iter.cmp.not, label %pfor.cond.cleanup, label %pfor.cond.epil, !llvm.loop !20

pfor.cond.cleanup:                                ; preds = %pfor.cond.epil, %middle.block28, %pfor.cond.cleanup.strpm-lcssa
  sync within %syncreg, label %cleanup

cleanup:                                          ; preds = %pfor.cond.cleanup, %entry
  ret void
}

; Function Attrs: mustprogress nounwind willreturn memory(argmem: readwrite)
declare token @llvm.syncregion.start() #1

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: mustprogress uwtable
define void @_Z3runm(i64 noundef %size) local_unnamed_addr #3 personality ptr @__gxx_personality_v0 {
entry:
  %eng = alloca %"class.std::mersenne_twister_engine", align 8
  %call1.i = tail call noundef nonnull align 8 dereferenceable(8) ptr @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, ptr noundef nonnull @.str, i64 noundef 27)
  %call.i = tail call noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo9_M_insertImEERSoT_(ptr noundef nonnull align 8 dereferenceable(8) @_ZSt4cout, i64 noundef %size)
  %call1.i73 = tail call noundef nonnull align 8 dereferenceable(8) ptr @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(ptr noundef nonnull align 8 dereferenceable(8) %call.i, ptr noundef nonnull @.str.1, i64 noundef 4)
  %vtable.i = load ptr, ptr %call.i, align 8, !tbaa !21
  %vbase.offset.ptr.i = getelementptr i8, ptr %vtable.i, i64 -24
  %vbase.offset.i = load i64, ptr %vbase.offset.ptr.i, align 8
  %add.ptr.i = getelementptr inbounds i8, ptr %call.i, i64 %vbase.offset.i
  %_M_ctype.i.i = getelementptr inbounds i8, ptr %add.ptr.i, i64 240
  %0 = load ptr, ptr %_M_ctype.i.i, align 8, !tbaa !23
  %tobool.not.i.i.i = icmp eq ptr %0, null
  br i1 %tobool.not.i.i.i, label %if.then.i.i.i158, label %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i

if.then.i.i.i158:                                 ; preds = %entry
  tail call void @_ZSt16__throw_bad_castv() #8
  unreachable

_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i: ; preds = %entry
  %_M_widen_ok.i.i.i = getelementptr inbounds i8, ptr %0, i64 56
  %1 = load i8, ptr %_M_widen_ok.i.i.i, align 8, !tbaa !33
  %tobool.not.i3.i.i = icmp eq i8 %1, 0
  br i1 %tobool.not.i3.i.i, label %if.end.i.i.i, label %if.then.i4.i.i

if.then.i4.i.i:                                   ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  %arrayidx.i.i.i = getelementptr inbounds i8, ptr %0, i64 67
  %2 = load i8, ptr %arrayidx.i.i.i, align 1, !tbaa !36
  br label %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit

if.end.i.i.i:                                     ; preds = %_ZSt13__check_facetISt5ctypeIcEERKT_PS3_.exit.i.i
  tail call void @_ZNKSt5ctypeIcE13_M_widen_initEv(ptr noundef nonnull align 8 dereferenceable(570) %0)
  %vtable.i.i.i = load ptr, ptr %0, align 8, !tbaa !21
  %vfn.i.i.i = getelementptr inbounds i8, ptr %vtable.i.i.i, i64 48
  %3 = load ptr, ptr %vfn.i.i.i, align 8
  %call.i.i.i = tail call noundef signext i8 %3(ptr noundef nonnull align 8 dereferenceable(570) %0, i8 noundef signext 10)
  br label %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit

_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit: ; preds = %if.then.i4.i.i, %if.end.i.i.i
  %retval.0.i.i.i = phi i8 [ %2, %if.then.i4.i.i ], [ %call.i.i.i, %if.end.i.i.i ]
  %call1.i156 = tail call noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo3putEc(ptr noundef nonnull align 8 dereferenceable(8) %call.i, i8 noundef signext %retval.0.i.i.i)
  %call.i.i157 = tail call noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo5flushEv(ptr noundef nonnull align 8 dereferenceable(8) %call1.i156)
  call void @llvm.lifetime.start.p0(i64 2504, ptr nonnull %eng) #9
  %call7 = tail call i64 @_ZNSt6chrono3_V212system_clock3nowEv() #9
  store i64 %call7, ptr %eng, align 8, !tbaa !4
  br label %for.body.i.i

for.body.i.i:                                     ; preds = %for.body.i.i, %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit
  %4 = phi i64 [ %call7, %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit ], [ %add.i.i, %for.body.i.i ]
  %__i.016.i.i = phi i64 [ 1, %_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_.exit ], [ %inc.i.i, %for.body.i.i ]
  %shr.i.i = lshr i64 %4, 62
  %xor.i.i = xor i64 %shr.i.i, %4
  %mul.i.i = mul i64 %xor.i.i, 6364136223846793005
  %add.i.i = add i64 %mul.i.i, %__i.016.i.i
  %arrayidx7.i.i = getelementptr inbounds [312 x i64], ptr %eng, i64 0, i64 %__i.016.i.i
  store i64 %add.i.i, ptr %arrayidx7.i.i, align 8, !tbaa !4
  %inc.i.i = add nuw nsw i64 %__i.016.i.i, 1
  %exitcond.not.i.i = icmp eq i64 %inc.i.i, 312
  br i1 %exitcond.not.i.i, label %invoke.cont15, label %for.body.i.i, !llvm.loop !37

invoke.cont15:                                    ; preds = %for.body.i.i
  %_M_p.i.i = getelementptr inbounds i8, ptr %eng, i64 2496
  store i64 312, ptr %_M_p.i.i, align 8, !tbaa !38
  %cmp174.not = icmp eq i64 %size, 0
  br i1 %cmp174.not, label %invoke.cont47.tfend, label %if.else12.i.i

if.else12.i.i:                                    ; preds = %invoke.cont15, %if.else12.i.i
  %indvars.iv = phi i64 [ %indvars.iv.next, %if.else12.i.i ], [ 0, %invoke.cont15 ]
  %call13.i.i85 = call noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %eng)
  %call13.i.i118 = call noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %eng)
  %indvars.iv.next = add nuw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %size
  br i1 %exitcond.not, label %invoke.cont47.tfend, label %if.else12.i.i, !llvm.loop !40

invoke.cont47.tfend:                              ; preds = %if.else12.i.i, %invoke.cont15
  %call43178 = call i32 @cudaProfilerStart()
  %call49 = call i32 @cudaProfilerStop()
  call void @llvm.lifetime.end.p0(i64 2504, ptr nonnull %eng) #9
  ret void
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nounwind
declare i64 @_ZNSt6chrono3_V212system_clock3nowEv() local_unnamed_addr #4

declare i32 @cudaProfilerStart() local_unnamed_addr #5

declare i32 @cudaProfilerStop() local_unnamed_addr #5

; Function Attrs: mustprogress norecurse uwtable
define noundef i32 @main() local_unnamed_addr #6 {
entry:
  tail call void @_Z3runm(i64 noundef 8388608)
  ret i32 0
}

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZSt16__ostream_insertIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_PKS3_l(ptr noundef nonnull align 8 dereferenceable(8), ptr noundef, i64 noundef) local_unnamed_addr #5

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo9_M_insertImEERSoT_(ptr noundef nonnull align 8 dereferenceable(8), i64 noundef) local_unnamed_addr #5

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo3putEc(ptr noundef nonnull align 8 dereferenceable(8), i8 noundef signext) local_unnamed_addr #5

declare noundef nonnull align 8 dereferenceable(8) ptr @_ZNSo5flushEv(ptr noundef nonnull align 8 dereferenceable(8)) local_unnamed_addr #5

; Function Attrs: noreturn
declare void @_ZSt16__throw_bad_castv() local_unnamed_addr #7

declare void @_ZNKSt5ctypeIcE13_M_widen_initEv(ptr noundef nonnull align 8 dereferenceable(570)) local_unnamed_addr #5

; Function Attrs: mustprogress uwtable
define linkonce_odr noundef i64 @_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EEclEv(ptr noundef nonnull align 8 dereferenceable(2504) %this) local_unnamed_addr #3 comdat align 2 {
entry:
  %_M_p = getelementptr inbounds i8, ptr %this, i64 2496
  %0 = load i64, ptr %_M_p, align 8, !tbaa !38
  %cmp = icmp ugt i64 %0, 311
  br i1 %cmp, label %vector.ph, label %if.end

vector.ph:                                        ; preds = %entry
  %.pre.i = load i64, ptr %this, align 8, !tbaa !4
  %vector.recur.init = insertelement <2 x i64> poison, i64 %.pre.i, i64 1
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %vector.recur = phi <2 x i64> [ %vector.recur.init, %vector.ph ], [ %wide.load, %vector.body ]
  %1 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %index
  %2 = or disjoint i64 %index, 1
  %3 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %2
  %wide.load = load <2 x i64>, ptr %3, align 8, !tbaa !4
  %4 = shufflevector <2 x i64> %vector.recur, <2 x i64> %wide.load, <2 x i32> <i32 1, i32 2>
  %5 = and <2 x i64> %4, <i64 -2147483648, i64 -2147483648>
  %6 = and <2 x i64> %wide.load, <i64 2147483646, i64 2147483646>
  %7 = or disjoint <2 x i64> %6, %5
  %8 = add nuw nsw i64 %index, 156
  %9 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %8
  %wide.load18 = load <2 x i64>, ptr %9, align 8, !tbaa !4
  %10 = lshr exact <2 x i64> %7, <i64 1, i64 1>
  %11 = xor <2 x i64> %10, %wide.load18
  %12 = and <2 x i64> %wide.load, <i64 1, i64 1>
  %13 = icmp eq <2 x i64> %12, zeroinitializer
  %14 = select <2 x i1> %13, <2 x i64> zeroinitializer, <2 x i64> <i64 -5403634167711393303, i64 -5403634167711393303>
  %15 = xor <2 x i64> %11, %14
  store <2 x i64> %15, ptr %1, align 8, !tbaa !4
  %index.next = add nuw i64 %index, 2
  %16 = icmp eq i64 %index.next, 156
  br i1 %16, label %vector.ph21, label %vector.body, !llvm.loop !41

vector.ph21:                                      ; preds = %vector.body
  %arrayidx19.phi.trans.insert.i = getelementptr inbounds i8, ptr %this, i64 1248
  %.pre74.i = load i64, ptr %arrayidx19.phi.trans.insert.i, align 8, !tbaa !4
  %vector.recur.init25 = insertelement <2 x i64> poison, i64 %.pre74.i, i64 1
  br label %vector.body23

vector.body23:                                    ; preds = %vector.body23, %vector.ph21
  %index24 = phi i64 [ 0, %vector.ph21 ], [ %index.next29, %vector.body23 ]
  %vector.recur26 = phi <2 x i64> [ %vector.recur.init25, %vector.ph21 ], [ %wide.load27, %vector.body23 ]
  %offset.idx = add i64 %index24, 156
  %17 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %offset.idx
  %18 = add i64 %index24, 157
  %19 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %18
  %wide.load27 = load <2 x i64>, ptr %19, align 8, !tbaa !4
  %20 = shufflevector <2 x i64> %vector.recur26, <2 x i64> %wide.load27, <2 x i32> <i32 1, i32 2>
  %21 = and <2 x i64> %20, <i64 -2147483648, i64 -2147483648>
  %22 = and <2 x i64> %wide.load27, <i64 2147483646, i64 2147483646>
  %23 = or disjoint <2 x i64> %22, %21
  %24 = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %index24
  %wide.load28 = load <2 x i64>, ptr %24, align 8, !tbaa !4
  %25 = lshr exact <2 x i64> %23, <i64 1, i64 1>
  %26 = xor <2 x i64> %25, %wide.load28
  %27 = and <2 x i64> %wide.load27, <i64 1, i64 1>
  %28 = icmp eq <2 x i64> %27, zeroinitializer
  %29 = select <2 x i1> %28, <2 x i64> zeroinitializer, <2 x i64> <i64 -5403634167711393303, i64 -5403634167711393303>
  %30 = xor <2 x i64> %26, %29
  store <2 x i64> %30, ptr %17, align 8, !tbaa !4
  %index.next29 = add nuw i64 %index24, 2
  %31 = icmp eq i64 %index.next29, 154
  br i1 %31, label %middle.block19, label %vector.body23, !llvm.loop !42

middle.block19:                                   ; preds = %vector.body23
  %vector.recur.extract30 = extractelement <2 x i64> %wide.load27, i64 1
  br label %for.body16.i

for.body16.i:                                     ; preds = %for.body16.i, %middle.block19
  %32 = phi i64 [ %33, %for.body16.i ], [ %vector.recur.extract30, %middle.block19 ]
  %__k12.072.i = phi i64 [ %add22.i, %for.body16.i ], [ 310, %middle.block19 ]
  %arrayidx19.i = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %__k12.072.i
  %and20.i = and i64 %32, -2147483648
  %add22.i = add nuw nsw i64 %__k12.072.i, 1
  %arrayidx23.i = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %add22.i
  %33 = load i64, ptr %arrayidx23.i, align 8, !tbaa !4
  %and24.i = and i64 %33, 2147483646
  %or25.i = or disjoint i64 %and24.i, %and20.i
  %add27.i = add nsw i64 %__k12.072.i, -156
  %arrayidx28.i = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %add27.i
  %34 = load i64, ptr %arrayidx28.i, align 8, !tbaa !4
  %shr29.i = lshr exact i64 %or25.i, 1
  %xor30.i = xor i64 %shr29.i, %34
  %and31.i = and i64 %33, 1
  %tobool32.not.i = icmp eq i64 %and31.i, 0
  %cond33.i = select i1 %tobool32.not.i, i64 0, i64 -5403634167711393303
  %xor34.i = xor i64 %xor30.i, %cond33.i
  store i64 %xor34.i, ptr %arrayidx19.i, align 8, !tbaa !4
  %exitcond73.not.i = icmp eq i64 %add22.i, 311
  br i1 %exitcond73.not.i, label %_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE11_M_gen_randEv.exit, label %for.body16.i, !llvm.loop !43

_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE11_M_gen_randEv.exit: ; preds = %for.body16.i
  %arrayidx42.i = getelementptr inbounds i8, ptr %this, i64 2488
  %35 = load i64, ptr %arrayidx42.i, align 8, !tbaa !4
  %and43.i = and i64 %35, -2147483648
  %36 = load i64, ptr %this, align 8, !tbaa !4
  %and46.i = and i64 %36, 2147483646
  %or47.i = or disjoint i64 %and46.i, %and43.i
  %arrayidx49.i = getelementptr inbounds i8, ptr %this, i64 1240
  %37 = load i64, ptr %arrayidx49.i, align 8, !tbaa !4
  %shr50.i = lshr exact i64 %or47.i, 1
  %xor51.i = xor i64 %shr50.i, %37
  %and52.i = and i64 %36, 1
  %tobool53.not.i = icmp eq i64 %and52.i, 0
  %cond54.i = select i1 %tobool53.not.i, i64 0, i64 -5403634167711393303
  %xor55.i = xor i64 %xor51.i, %cond54.i
  store i64 %xor55.i, ptr %arrayidx42.i, align 8, !tbaa !4
  br label %if.end

if.end:                                           ; preds = %_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE11_M_gen_randEv.exit, %entry
  %38 = phi i64 [ 0, %_ZNSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE11_M_gen_randEv.exit ], [ %0, %entry ]
  %inc = add nuw nsw i64 %38, 1
  store i64 %inc, ptr %_M_p, align 8, !tbaa !38
  %arrayidx = getelementptr inbounds [312 x i64], ptr %this, i64 0, i64 %38
  %39 = load i64, ptr %arrayidx, align 8, !tbaa !4
  %shr = lshr i64 %39, 29
  %and = and i64 %shr, 22906492245
  %xor = xor i64 %and, %39
  %shl = shl i64 %xor, 17
  %and3 = and i64 %shl, 8202884508482404352
  %xor4 = xor i64 %and3, %xor
  %shl5 = shl i64 %xor4, 37
  %and6 = and i64 %shl5, -2270628950310912
  %xor7 = xor i64 %and6, %xor4
  %shr8 = lshr i64 %xor7, 43
  %xor9 = xor i64 %shr8, %xor7
  ret i64 %xor9
}

attributes #0 = { mustprogress nounwind memory(argmem: readwrite) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { mustprogress nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #3 = { mustprogress uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { mustprogress norecurse uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { noreturn "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #8 = { noreturn }
attributes #9 = { nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
!3 = !{!"clang version 19.1.7 (git@github.com:ma-chengyuan/opencilk-project.git b644df12986682b8dda843542a65839916798245)"}
!4 = !{!5, !5, i64 0}
!5 = !{!"long", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C++ TBAA"}
!8 = distinct !{!8, !9, !10, !11, !12, !13, !14}
!9 = !{!"llvm.loop.mustprogress"}
!10 = !{!"tapir.loop.target", i32 2}
!11 = !{!"llvm.loop.unroll.disable"}
!12 = !{!"llvm.loop.fromtapirloop"}
!13 = !{!"llvm.loop.isvectorized", i32 1}
!14 = !{!"llvm.loop.unroll.runtime.disable"}
!15 = distinct !{!15, !9, !10, !11, !12, !13}
!16 = distinct !{!16, !9, !17, !10, !11, !18}
!17 = !{!"tapir.loop.spawn.strategy", i32 1}
!18 = !{!"tapir.loop.grainsize", i32 1}
!19 = distinct !{!19, !12, !9, !11, !13, !14}
!20 = distinct !{!20, !12, !9, !11, !13}
!21 = !{!22, !22, i64 0}
!22 = !{!"vtable pointer", !7, i64 0}
!23 = !{!24, !28, i64 240}
!24 = !{!"_ZTSSt9basic_iosIcSt11char_traitsIcEE", !25, i64 0, !28, i64 216, !6, i64 224, !32, i64 225, !28, i64 232, !28, i64 240, !28, i64 248, !28, i64 256}
!25 = !{!"_ZTSSt8ios_base", !5, i64 8, !5, i64 16, !26, i64 24, !27, i64 28, !27, i64 32, !28, i64 40, !29, i64 48, !6, i64 64, !30, i64 192, !28, i64 200, !31, i64 208}
!26 = !{!"_ZTSSt13_Ios_Fmtflags", !6, i64 0}
!27 = !{!"_ZTSSt12_Ios_Iostate", !6, i64 0}
!28 = !{!"any pointer", !6, i64 0}
!29 = !{!"_ZTSNSt8ios_base6_WordsE", !28, i64 0, !5, i64 8}
!30 = !{!"int", !6, i64 0}
!31 = !{!"_ZTSSt6locale", !28, i64 0}
!32 = !{!"bool", !6, i64 0}
!33 = !{!34, !6, i64 56}
!34 = !{!"_ZTSSt5ctypeIcE", !35, i64 0, !28, i64 16, !32, i64 24, !28, i64 32, !28, i64 40, !28, i64 48, !6, i64 56, !6, i64 57, !6, i64 313, !6, i64 569}
!35 = !{!"_ZTSNSt6locale5facetE", !30, i64 8}
!36 = !{!6, !6, i64 0}
!37 = distinct !{!37, !9, !11}
!38 = !{!39, !5, i64 2496}
!39 = !{!"_ZTSSt23mersenne_twister_engineImLm64ELm312ELm156ELm31ELm13043109905998158313ELm29ELm6148914691236517205ELm17ELm8202884508482404352ELm37ELm18444473444759240704ELm43ELm6364136223846793005EE", !6, i64 0, !5, i64 2496}
!40 = distinct !{!40, !9, !11}
!41 = distinct !{!41, !9, !11, !13, !14}
!42 = distinct !{!42, !9, !11, !13, !14}
!43 = distinct !{!43, !9, !11, !13}
