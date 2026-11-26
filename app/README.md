# Dewdrop demo app

Mobile-first React/Vite dashboard for the existing Dewdrop Supabase project.
It supports Supabase admin sign-in, user selection by profile name, daily
hydration progress, editable targets, weekly history, and character status.

The selected profile's `device_id` filters `daily_water_progress` and
`water_logs`; its `character_id` filters `character_logs.device_id`.
`daily_water_progress` supplies today's total, with `water_logs` used for
history and as a fallback if today's progress row is absent.
History can be navigated by week; the displayed range includes the year.

## Local development

1. Copy `.env.example` to `.env.local` (Vite does not load `.env.example`).
2. Set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY`. Use the
   publishable key only; never put a Supabase `service_role` key in this app.
3. Create the admin account in Supabase Auth. Disable public sign-ups and
   configure email confirmation as intended for the demo.
4. Review `migration.sql` before applying it. In particular, verify IoT
   ingestion uses `service_role` or privileged trigger functions, since the
   migration enables RLS on the log tables.
5. Run `npm install`, then `npm run dev`.

The app uses `Europe/Copenhagen` calendar boundaries for daily and weekly
hydration totals. Profiles with no saved target use a 1,600 ml display default;
saving a target updates the selected profile.

## Cloudflare Pages

- Root directory: `app`
- Build command: `npm run build`
- Build output directory: `dist`
- Set `VITE_SUPABASE_URL` and `VITE_SUPABASE_PUBLISHABLE_KEY` in the Pages
  environment variables for both preview and production.
- Set the Supabase Auth Site URL and redirect URLs to the deployed Pages URL.

Vite embeds `VITE_` values into the public frontend bundle. The publishable key
is expected to be public; security comes from Supabase Auth and RLS policies.
