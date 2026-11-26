## Demo scope

- Admin signs in with Supabase email/password, then selects a person by
  `user_profile.name`.
- The selected profile's `device_id` is used for `daily_water_progress` and
  `water_logs`; its `character_id` is matched to `character_logs.device_id`.
- Overview shows today's water intake, progress against the editable daily
  target, recent drinking activity, and the latest companion state.
- Today's progress comes from `daily_water_progress`; weekly history is
  aggregated from `water_logs` for a navigable Monday–Sunday week, with its
  full date range shown in the history screen.
- Companion view shows the character state and today's progress.
- Character states come from the latest matching `character_logs` row:
  `0 = sad`, `1 = neutral`, `2 = happy`, `3 = target`.
- `water_logs.water_drank` and `user_profile.daily_water_target` are displayed
  in millilitres. Day boundaries use `Europe/Copenhagen`.

## Implementation and deployment

- Frontend: React + TypeScript + Vite in this folder.
- Backend: existing Supabase tables; no IoT trigger logic is changed.
- Access: one authenticated admin, with RLS policies described in
  `migration.sql`. The admin account is created separately in Supabase Auth.
- Hosting: Cloudflare Pages, built with `npm run build` to `dist`.
- Configure `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY` as Pages
  environment variables. Never expose a service-role key in the browser.
