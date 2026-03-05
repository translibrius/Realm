# Boot and Frame Loop

```
main.c → create_application()
  rl_engine_create()          # memory → events → logger → platform → input → assets
  platform_create_window()
  renderer_init()
  realm_app_module_load/init()
  realm_app_watcher_start()

loop:
  rl_engine_begin_frame()     # input_update, platform_pump_messages, renderer_begin_frame
  app_module.update()
  app_module.render()         # calls renderer_submit_frame_data(rl_frame_data*)
  rl_engine_end_frame()       # renderer_end_frame, swap_buffers, rl_arena_clear
```

## Runtime hotkeys

- **F5** → rebuild + reload app module.
- **F10** → switch backend (destroys renderer + window, recreates both, then reloads app module).
