-- Dewdrop app access migration.
-- Create the admin account in Supabase Auth separately; never store its password here.
-- This grants app access only to the authenticated admin email below.
-- The dashboard also reads the existing daily_water_progress view/table.
--
-- IoT ingestion must use the Supabase service_role (which bypasses RLS), or its
-- trigger functions must run with a privileged owner. If ingestion uses anon,
-- enabling RLS here will block its inserts into water_logs/character_logs.
-- Confirm that before applying this migration; the IoT triggers are not changed.

begin;

create index if not exists water_logs_device_time_drank_idx
  on public.water_logs (device_id, time_drank desc);

create index if not exists character_logs_device_time_received_idx
  on public.character_logs (device_id, time_received desc);

alter table public.user_profile enable row level security;
alter table public.water_logs enable row level security;
alter table public.character_logs enable row level security;

grant select, insert, update on public.user_profile to authenticated;
grant select on public.water_logs to authenticated;
grant select on public.character_logs to authenticated;
grant select on public.daily_water_progress to authenticated;

create policy dewdrop_admin_profile_select
  on public.user_profile
  for select
  to authenticated
  using ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com');

create policy dewdrop_admin_profile_insert
  on public.user_profile
  for insert
  to authenticated
  with check ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com');

create policy dewdrop_admin_profile_update
  on public.user_profile
  for update
  to authenticated
  using ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com')
  with check ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com');

create policy dewdrop_admin_water_logs_select
  on public.water_logs
  for select
  to authenticated
  using ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com');

create policy dewdrop_admin_character_logs_select
  on public.character_logs
  for select
  to authenticated
  using ((auth.jwt() ->> 'email') = 'dewdrop@gmail.com');

commit;
