; ModuleID = 'hwarangdo'
source_filename = "hwarangdo"

%Test = type {}
%Main = type {}

declare void @hrd_destroy_s8(ptr)

define void @Maindestroy.field(ptr %0) {
entry:
  ret void
}

define void @Testdestroy.field(ptr %0) {
entry:
  ret void
}

define void @Maindefault.init.field(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  ret void
}

define void @Main_update(ptr %self) {
entry:
  %h3 = alloca i64, align 8
  %h2 = alloca i64, align 8
  %h1 = alloca i64, align 8
  br label %bb0

bb0:                                              ; preds = %entry
  %0 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%Test, ptr null, i32 1) to i64))
  call void @llvm.memset.p0.i64(ptr align 8 %0, i8 0, i64 ptrtoint (ptr getelementptr (%Test, ptr null, i32 1) to i64), i1 false)
  call void @Testdefault.init.field(ptr %0)
  %1 = call i64 @hrd_world_spawn_raw(ptr %0, ptr @Testdestroy.field, ptr null)
  store i64 %1, ptr %h1, align 4
  %loadtmp = load i64, ptr %h1, align 4
  store i64 %loadtmp, ptr %h2, align 4
  %2 = call ptr @malloc(i64 ptrtoint (ptr getelementptr (%Test, ptr null, i32 1) to i64))
  call void @llvm.memset.p0.i64(ptr align 8 %2, i8 0, i64 ptrtoint (ptr getelementptr (%Test, ptr null, i32 1) to i64), i1 false)
  call void @Testdefault.init.field(ptr %2)
  %3 = call i64 @hrd_world_spawn_raw(ptr %2, ptr @Testdestroy.field, ptr null)
  store i64 %3, ptr %h3, align 4
  %loadtmp1 = load i64, ptr %h1, align 4
  %loadtmp2 = load i64, ptr %h2, align 4
  %4 = icmp eq i64 %loadtmp1, %loadtmp2
  call void @hrd_log_info_bool(i1 %4)
  %loadtmp3 = load i64, ptr %h1, align 4
  %loadtmp4 = load i64, ptr %h3, align 4
  %5 = icmp eq i64 %loadtmp3, %loadtmp4
  call void @hrd_log_info_bool(i1 %5)
  %loadtmp5 = load i64, ptr %h1, align 4
  call void @hrd_world_destroy_entity_raw(i64 %loadtmp5)
  %loadtmp6 = load i64, ptr %h3, align 4
  call void @hrd_world_destroy_entity_raw(i64 %loadtmp6)
  call void @hrd_world_quit()
  ret void
}

define void @Main_init(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  ret void
}

define void @Testdefault.init.field(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  ret void
}

declare ptr @malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: write)
declare void @llvm.memset.p0.i64(ptr writeonly captures(none), i8, i64, i1 immarg) #0

declare i64 @hrd_world_spawn_raw(ptr, ptr, ptr)

declare void @hrd_log_info_bool(i1)

declare void @hrd_world_destroy_entity_raw(i64)

declare void @hrd_world_quit()

define i32 @main() {
entry:
  %0 = call ptr @hrd_world_create()
  %1 = alloca %Main, align 8
  call void @Maindefault.init.field(ptr %1)
  call void @Main_init(ptr %1)
  br label %loop.cond

loop.cond:                                        ; preds = %loop.body, %entry
  %2 = call i1 @hrd_world_running()
  br i1 %2, label %loop.body, label %exit

loop.body:                                        ; preds = %loop.cond
  call void @Main_update(ptr %1)
  call void @hrd_world_flush_destroy()
  br label %loop.cond

exit:                                             ; preds = %loop.cond
  call void @Maindestroy.field(ptr %1)
  call void @hrd_world_destroy()
  ret i32 0
}

declare ptr @hrd_world_create()

declare i1 @hrd_world_running()

declare void @hrd_world_destroy()

declare void @hrd_world_flush_destroy()

attributes #0 = { nocallback nofree nounwind willreturn memory(argmem: write) }
