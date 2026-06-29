; ModuleID = 'hwarangdo'
source_filename = "hwarangdo"

%string8 = type { ptr, i64 }
%Main = type {}

@0 = private unnamed_addr constant [13 x i8] c"hello world!\00", align 1

define void @Main_update(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  ret void
}

define void @Main_init(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  call void @hrd_log_info_s8(%string8 { ptr @0, i64 12 })
  ret void
}

declare void @hrd_log_info_s8(%string8)

define i32 @main() {
entry:
  %0 = alloca %Main, align 8
  call void @Main_init(ptr %0)
  call void @Main_update(ptr %0)
  ret i32 0
}
