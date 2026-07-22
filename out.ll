; ModuleID = 'hwarangdo'
source_filename = "hwarangdo"

%Main = type {}

declare void @hrd_destroy_s8(ptr)

define void @Maindestroy.field(ptr %0) {
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
  br label %bb0

bb0:                                              ; preds = %entry
  call void @hrd_world_quit()
  ret void
}

define void @Main_init(ptr %self) {
entry:
  br label %bb0

bb0:                                              ; preds = %entry
  ret void
}

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
