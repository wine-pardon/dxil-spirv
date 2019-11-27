#version 460

layout(location = 0) in vec4 A;
layout(location = 1) in vec4 B;
layout(location = 0) out vec4 SV_Target;

void main()
{
    SV_Target.x = A.x + B.x;
    SV_Target.y = A.y + B.y;
    SV_Target.z = A.z + B.z;
    SV_Target.w = A.w + B.w;
}

#if 0
// LLVM disassembly
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

define void @main() {
  %1 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %2 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)
  %3 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 2, i32 undef)
  %4 = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 3, i32 undef)
  %5 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %6 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %7 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)
  %8 = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)
  %.i0 = fadd fast float %5, %1
  %.i1 = fadd fast float %6, %2
  %.i2 = fadd fast float %7, %3
  %.i3 = fadd fast float %8, %4
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %.i0)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %.i1)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %.i2)
  call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %.i3)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #0

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #1

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.typeAnnotations = !{!4}
!dx.viewIdState = !{!8}
!dx.entryPoints = !{!9}

!0 = !{!"dxcoob 2019.05.00"}
!1 = !{i32 1, i32 0}
!2 = !{i32 1, i32 4}
!3 = !{!"ps", i32 6, i32 0}
!4 = !{i32 1, void ()* @main, !5}
!5 = !{!6}
!6 = !{i32 0, !7, !7}
!7 = !{}
!8 = !{[10 x i32] [i32 8, i32 4, i32 1, i32 2, i32 4, i32 8, i32 1, i32 2, i32 4, i32 8]}
!9 = !{void ()* @main, !"main", !10, null, null}
!10 = !{!11, !15, null}
!11 = !{!12, !14}
!12 = !{i32 0, !"A", i8 9, i8 0, !13, i8 2, i32 1, i8 4, i32 0, i8 0, null}
!13 = !{i32 0}
!14 = !{i32 1, !"B", i8 9, i8 0, !13, i8 2, i32 1, i8 4, i32 1, i8 0, null}
!15 = !{!16}
!16 = !{i32 0, !"SV_Target", i8 9, i8 16, !13, i8 0, i32 1, i8 4, i32 0, i8 0, null}
#endif