import { useCallback, useEffect, useMemo, useState } from "react";
import type { ChangeEvent, FormEvent } from "react";
import type { Session } from "@supabase/supabase-js";
import happyImage from "../assets/happy.png";
import neutralImage from "../assets/neutral.png";
import sadImage from "../assets/sad.png";
import targetImage from "../assets/Target.png";
import { supabase } from "./lib/supabase";
import {
  copenhagenDateKey,
  copenhagenWeek,
  formatDateRange,
  formatShortDate,
  formatTime,
} from "./lib/time";

type WaterLog = {
  id: string;
  device_id: string;
  time_drank: string;
  water_drank: number;
};

type CharacterLog = {
  pet_state: number;
  pet_mood: string | null;
  time_received: string;
};

type UserProfile = {
  id: string;
  name: string | null;
  device_id: string;
  character_id: string;
  daily_water_target: number | null;
};

type Tab = "overview" | "history" | "companion";

const DEFAULT_TARGET = 1600;
const numberFormat = new Intl.NumberFormat("en-GB", {
  maximumFractionDigits: 0,
});

const petStates: Record<number, { name: string; message: string; image: string }> = {
  0: {
    name: "Sad",
    message: "A little sip is a lovely start.",
    image: sadImage,
  },
  1: {
    name: "Neutral",
    message: "Keep going, one sip at a time.",
    image: neutralImage,
  },
  2: {
    name: "Happy",
    message: "Your companion is feeling great!",
    image: happyImage,
  },
  3: {
    name: "Goal reached",
    message: "You reached your hydration goal!",
    image: targetImage,
  },
};

function formatMillilitres(value: number): string {
  return `${numberFormat.format(Math.round(value))} ml`;
}

function formatLitres(value: number): string {
  return `${(value / 1000).toFixed(1)} L`;
}

function profileDisplayName(profile: UserProfile): string {
  return profile.name?.trim() || `Unnamed profile (${profile.device_id})`;
}

function App() {
  const [session, setSession] = useState<Session | null>(null);
  const [authLoading, setAuthLoading] = useState(true);

  useEffect(() => {
    if (!supabase) {
      setAuthLoading(false);
      return;
    }

    void supabase.auth.getSession().then(({ data, error }) => {
      if (error) {
        console.error("Could not restore the Supabase session.", error);
      }
      setSession(data.session);
      setAuthLoading(false);
    });

    const {
      data: { subscription },
    } = supabase.auth.onAuthStateChange((_event, nextSession) => {
      setSession(nextSession);
      setAuthLoading(false);
    });

    return () => subscription.unsubscribe();
  }, []);

  if (!supabase) {
    return (
      <MessageScreen
        title="Connect Supabase"
        message="Add VITE_SUPABASE_URL and VITE_SUPABASE_PUBLISHABLE_KEY to your app environment, then restart the dev server."
      />
    );
  }

  if (authLoading) {
    return <MessageScreen title="Dewdrop" message="Restoring your session…" />;
  }

  if (!session) {
    return <LoginPage />;
  }

  return <Dashboard email={session.user.email ?? "Admin"} />;
}

function MessageScreen({ title, message }: { title: string; message: string }) {
  return (
    <main className="message-screen">
      <section className="message-card">
        <Brand />
        <h1>{title}</h1>
        <p>{message}</p>
      </section>
    </main>
  );
}

function Brand() {
  return (
    <div className="brand">
      <span className="brand-name">dewdrop</span>
      <span className="brand-tagline">A drop at a time</span>
    </div>
  );
}

function LoginPage() {
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!supabase) return;

    setBusy(true);
    setError("");
    const { error: signInError } = await supabase.auth.signInWithPassword({
      email: email.trim(),
      password,
    });
    setBusy(false);
    if (signInError) setError(signInError.message);
  }

  return (
    <main className="login-screen">
      <section className="login-card">
        <Brand />
        <p className="eyebrow">ADMIN ACCESS</p>
        <h1>Welcome back</h1>
        <p className="muted">Sign in to view your connected hydration devices.</p>
        <form className="form-stack" onSubmit={handleSubmit}>
          <label>
            Email
            <input
              autoComplete="username"
              type="email"
              value={email}
              onChange={(event) => setEmail(event.target.value)}
              required
            />
          </label>
          <label>
            Password
            <input
              autoComplete="current-password"
              type="password"
              value={password}
              onChange={(event) => setPassword(event.target.value)}
              required
            />
          </label>
          {error && <p className="error-message" role="alert">{error}</p>}
          <button className="primary-button" disabled={busy} type="submit">
            {busy ? "Signing in…" : "Sign in"}
          </button>
        </form>
      </section>
    </main>
  );
}

function Dashboard({ email }: { email: string }) {
  const [profiles, setProfiles] = useState<UserProfile[]>([]);
  const [selectedProfileId, setSelectedProfileId] = useState(
    () => sessionStorage.getItem("dewdrop.profileId") ?? "",
  );
  const [profilesLoading, setProfilesLoading] = useState(true);
  const [activeTab, setActiveTab] = useState<Tab>("overview");
  const [weekLogs, setWeekLogs] = useState<WaterLog[]>([]);
  const [pet, setPet] = useState<CharacterLog | null>(null);
  const [target, setTarget] = useState(DEFAULT_TARGET);
  const [targetInput, setTargetInput] = useState(String(DEFAULT_TARGET));
  const [todayTotal, setTodayTotal] = useState(0);
  const [editingTarget, setEditingTarget] = useState(false);
  const [loading, setLoading] = useState(false);
  const [savingTarget, setSavingTarget] = useState(false);
  const [error, setError] = useState("");
  const [notice, setNotice] = useState("");
  const [now, setNow] = useState(() => new Date());
  const [weekOffset, setWeekOffset] = useState(0);

  const selectedProfile =
    profiles.find((profile) => profile.id === selectedProfileId) ?? null;
  const deviceId = selectedProfile?.device_id ?? "";
  const characterId = selectedProfile?.character_id ?? "";
  const week = useMemo(() => copenhagenWeek(now, weekOffset), [now, weekOffset]);
  const todayKey = copenhagenDateKey(now);

  useEffect(() => {
    let active = true;

    async function loadProfiles() {
      const client = supabase;
      if (!client) return;
      setProfilesLoading(true);
      setError("");
      const { data, error: profileError } = await client
        .from("user_profile")
        .select("id, name, device_id, character_id, daily_water_target")
        .order("name", { ascending: true });

      if (!active) return;
      if (profileError) {
        setError(profileError.message);
        setProfiles([]);
      } else {
        setProfiles((data ?? []) as UserProfile[]);
      }
      setProfilesLoading(false);
    }

    void loadProfiles();
    return () => {
      active = false;
    };
  }, []);

  useEffect(() => {
    if (profilesLoading) return;
    if (selectedProfileId && !profiles.some((profile) => profile.id === selectedProfileId)) {
      setSelectedProfileId("");
      sessionStorage.removeItem("dewdrop.profileId");
    }
  }, [profiles, profilesLoading, selectedProfileId]);

  useEffect(() => {
    const savedTarget = Number(selectedProfile?.daily_water_target);
    const nextTarget = Number.isFinite(savedTarget) && savedTarget > 0
      ? savedTarget
      : DEFAULT_TARGET;
    setTarget(nextTarget);
    setTargetInput(String(nextTarget));
  }, [selectedProfile]);

  const todayLogs = useMemo(
    () => weekLogs.filter((log) => copenhagenDateKey(new Date(log.time_drank)) === todayKey),
    [todayKey, weekLogs],
  );
  const progress = target > 0 ? Math.min((todayTotal / target) * 100, 100) : 0;
  const latestActivity = todayLogs.slice(0, 3);
  const state = pet ? petStates[pet.pet_state] : undefined;

  const loadData = useCallback(async () => {
    if (!supabase || !selectedProfile) return;
    setLoading(true);
    setError("");

    const [waterResult, characterResult, progressResult] = await Promise.all([
      supabase
        .from("water_logs")
        .select("id, device_id, time_drank, water_drank")
        .eq("device_id", deviceId)
        .gte("time_drank", week.range.start.toISOString())
        .lt("time_drank", week.range.end.toISOString())
        .order("time_drank", { ascending: false }),
      supabase
        .from("character_logs")
        .select("pet_state, pet_mood, time_received")
        .eq("device_id", characterId)
        .order("time_received", { ascending: false })
        .limit(1)
        .maybeSingle(),
      supabase
        .from("daily_water_progress")
        .select("total_drank")
        .eq("device_id", deviceId)
        .eq("log_date", todayKey)
        .maybeSingle(),
    ]);

    const requestError =
      waterResult.error ?? characterResult.error ?? progressResult.error;
    if (requestError) {
      setError(requestError.message);
      setLoading(false);
      return;
    }

    const logs = (waterResult.data ?? []) as WaterLog[];
    setWeekLogs(logs);
    setPet(characterResult.data as CharacterLog | null);
    const todayFromLogs = logs
      .filter((log) => copenhagenDateKey(new Date(log.time_drank)) === todayKey)
      .reduce((sum, log) => sum + Number(log.water_drank), 0);
    const progressTotal = Number(progressResult.data?.total_drank);
    setTodayTotal(Number.isFinite(progressTotal) ? progressTotal : todayFromLogs);
    setLoading(false);
  }, [
    characterId,
    deviceId,
    selectedProfile,
    todayKey,
    week.range.end,
    week.range.start,
  ]);

  useEffect(() => {
    if (!selectedProfile) return;
    void loadData();
    const intervalId = window.setInterval(() => {
      setNow(new Date());
      void loadData();
    }, 30_000);
    return () => window.clearInterval(intervalId);
  }, [loadData, selectedProfile]);

  function handleProfileChange(event: ChangeEvent<HTMLSelectElement>) {
    const nextProfileId = event.target.value;
    setError("");
    setNotice("");
    setWeekLogs([]);
    setPet(null);
    setTodayTotal(0);
    setEditingTarget(false);
    setSelectedProfileId(nextProfileId);
    if (nextProfileId) {
      sessionStorage.setItem("dewdrop.profileId", nextProfileId);
    } else {
      sessionStorage.removeItem("dewdrop.profileId");
    }
  }

  async function handleTargetSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!supabase || !selectedProfile) return;

    const nextTarget = Number(targetInput);
    if (!Number.isFinite(nextTarget) || nextTarget <= 0 || nextTarget > 20_000) {
      setError("Enter a target between 1 and 20,000 ml.");
      return;
    }

    setSavingTarget(true);
    setError("");
    setNotice("");
    const { error: saveError } = await supabase
      .from("user_profile")
      .update({ daily_water_target: nextTarget })
      .eq("id", selectedProfile.id);
    setSavingTarget(false);

    if (saveError) {
      setError(saveError.message);
      return;
    }

    setTarget(nextTarget);
    setProfiles((currentProfiles) =>
      currentProfiles.map((profile) =>
        profile.id === selectedProfile.id
          ? { ...profile, daily_water_target: nextTarget }
          : profile,
      ),
    );
    setEditingTarget(false);
    setNotice("Daily target saved.");
    await loadData();
  }

  async function handleSignOut() {
    if (!supabase) return;
    const { error: signOutError } = await supabase.auth.signOut();
    if (signOutError) setError(signOutError.message);
    else {
      sessionStorage.removeItem("dewdrop.profileId");
      setSelectedProfileId("");
    }
  }

  const weekTotals = week.dateKeys.map((dateKey) => ({
    dateKey,
    total: dateKey === todayKey
      ? todayTotal
      : weekLogs
          .filter((log) => copenhagenDateKey(new Date(log.time_drank)) === dateKey)
          .reduce((sum, log) => sum + Number(log.water_drank), 0),
  }));
  const average = weekTotals.reduce((sum, day) => sum + day.total, 0) / 7;
  const goalDays = weekTotals.filter((day) => day.total >= target).length;
  const chartScale = Math.max(target, ...weekTotals.map((day) => day.total), 1);

  return (
    <div className="app-shell">
      <header className="topbar">
        <Brand />
        <div className="account-actions">
          <span className="account-email">{email}</span>
          <button className="text-button" onClick={handleSignOut} type="button">
            Sign out
          </button>
        </div>
      </header>

      <main className="page-content">
        <section className="device-bar">
          <div>
            <p className="eyebrow">HYDRATION PROFILE</p>
            <h1>
              {selectedProfile
                ? `${profileDisplayName(selectedProfile)}’s day`
                : "Choose a user"}
            </h1>
          </div>
          <label className="profile-picker">
            <span>Select user</span>
            <select
              aria-label="Select user"
              value={selectedProfileId}
              onChange={handleProfileChange}
              disabled={profilesLoading}
            >
              <option value="">
                {profilesLoading ? "Loading users…" : "Choose a user"}
              </option>
              {profiles.map((profile) => (
                <option key={profile.id} value={profile.id}>
                  {profileDisplayName(profile)}
                </option>
              ))}
            </select>
          </label>
        </section>

        {error && <p className="error-message page-alert" role="alert">{error}</p>}
        {notice && <p className="success-message page-alert" role="status">{notice}</p>}

        {!selectedProfile ? (
          <section className="empty-device card">
            <div className="drop-icon" aria-hidden="true">⌖</div>
            <h2>{profiles.length ? "Select a user" : "No users found"}</h2>
            <p className="muted">
              {profilesLoading
                ? "Loading user profiles…"
                : profiles.length
                  ? "Choose a name above to view hydration and companion status."
                  : "Add a row to user_profile with a name to get started."}
            </p>
          </section>
        ) : (
          <>
            <nav className="desktop-tabs" aria-label="App sections">
              <TabButton tab="overview" activeTab={activeTab} onSelect={setActiveTab} />
              <TabButton tab="history" activeTab={activeTab} onSelect={setActiveTab} />
              <TabButton tab="companion" activeTab={activeTab} onSelect={setActiveTab} />
            </nav>

            {loading && weekLogs.length === 0 ? (
              <p className="loading-label">Loading device data…</p>
            ) : activeTab === "overview" ? (
              <section className="overview-grid">
                <div className="overview-main">
                  <div className="page-heading">
                    <div>
                      <p className="eyebrow">TODAY · {formatShortDate(todayKey).toUpperCase()}</p>
                      <h2>{profileDisplayName(selectedProfile)}’s day</h2>
                    </div>
                    <span className={`status-pill ${progress >= 100 ? "status-good" : ""}`}>
                      {progress >= 100 ? "Goal reached" : todayTotal > 0 ? "On track" : "Ready"}
                    </span>
                  </div>

                  <section className="progress-card card">
                    <div className="card-title-row">
                      <h3>Today’s hydration</h3>
                      <button
                        className="text-button"
                        onClick={() => {
                          setTargetInput(String(target));
                          setEditingTarget((value) => !value);
                          setError("");
                        }}
                        type="button"
                      >
                        {editingTarget ? "Cancel" : "Edit target"}
                      </button>
                    </div>
                    <div
                      className="progress-ring"
                      role="progressbar"
                      aria-label="Daily hydration progress"
                      aria-valuemin={0}
                      aria-valuemax={target}
                      aria-valuenow={Math.min(todayTotal, target)}
                      style={{ background: `conic-gradient(var(--blue) ${progress}%, var(--ring-bg) 0)` }}
                    >
                      <div className="progress-ring-inner">
                        <strong>{formatMillilitres(todayTotal)}</strong>
                        <span>of {formatMillilitres(target)}</span>
                      </div>
                    </div>
                    <p className="progress-caption">
                      {todayTotal >= target
                        ? "A wonderful day for hydration"
                        : todayTotal > 0
                          ? "A good pace for today"
                          : "Every sip is a fresh start"}
                    </p>
                    <div className="remaining">
                      {todayTotal >= target
                        ? "Daily target reached"
                        : `${formatMillilitres(target - todayTotal)} to go today`}
                    </div>
                    {editingTarget && (
                      <form className="target-editor" onSubmit={handleTargetSubmit}>
                        <label htmlFor="daily-target">Daily target (ml)</label>
                        <div className="target-input-row">
                          <input
                            id="daily-target"
                            type="number"
                            min="1"
                            max="20000"
                            step="50"
                            value={targetInput}
                            onChange={(event) => setTargetInput(event.target.value)}
                            required
                          />
                          <button className="primary-button compact" disabled={savingTarget}>
                            {savingTarget ? "Saving…" : "Save"}
                          </button>
                        </div>
                      </form>
                    )}
                  </section>

                  <section className="activity-section">
                    <div className="section-title-row">
                      <h3>Latest activity</h3>
                      <button
                        className="text-button"
                        onClick={() => setActiveTab("history")}
                        type="button"
                      >
                        View history
                      </button>
                    </div>
                    {latestActivity.length ? (
                      <div className="activity-list">
                        {latestActivity.map((log) => (
                          <ActivityRow key={log.id} log={log} />
                        ))}
                      </div>
                    ) : (
                      <div className="card empty-state">
                        {loading ? "Refreshing hydration data…" : "No drinks recorded for this day yet."}
                      </div>
                    )}
                  </section>
                </div>

                <CompanionCard state={state} pet={pet} target={target} total={todayTotal} />
              </section>
            ) : activeTab === "history" ? (
              <section className="history-page">
                <p className="eyebrow">HYDRATION HISTORY</p>
                <h2>Weekly hydration</h2>
                <div className="week-navigation">
                  <button
                    className="week-nav-button"
                    aria-label="Previous week"
                    onClick={() => setWeekOffset((offset) => offset - 1)}
                    type="button"
                  >
                    ‹
                  </button>
                  <div className="week-range-label" aria-live="polite">
                    <span>{weekOffset === 0 ? "CURRENT WEEK" : "SHOWING WEEK"}</span>
                    <strong>
                      {formatDateRange(week.dateKeys[0], week.dateKeys[6])}
                    </strong>
                  </div>
                  <div className="week-nav-actions">
                    {weekOffset !== 0 && (
                      <button
                        className="text-button"
                        onClick={() => setWeekOffset(0)}
                        type="button"
                      >
                        Today
                      </button>
                    )}
                    <button
                      className="week-nav-button"
                      aria-label="Next week"
                      disabled={weekOffset >= 0}
                      onClick={() => setWeekOffset((offset) => Math.min(offset + 1, 0))}
                      type="button"
                    >
                      ›
                    </button>
                  </div>
                </div>
                <div className="history-summary">
                  <div className="card summary-card">
                    <strong>{formatLitres(average)}</strong>
                    <span>Daily average</span>
                  </div>
                  <div className="card summary-card">
                    <strong>{goalDays} of 7</strong>
                    <span>Days at goal</span>
                  </div>
                </div>
                <section className="card chart-card">
                  <div className="chart-heading">
                    <div>
                      <h3>{weekOffset === 0 ? "This week" : "Selected week"}</h3>
                      <p className="muted">Daily goal: {formatMillilitres(target)}</p>
                    </div>
                    <div className="chart-today">
                      <span>Today</span>
                      <strong>{formatMillilitres(todayTotal)}</strong>
                    </div>
                  </div>
                  <div className="bar-chart" aria-label="Hydration by day this week">
                    {weekTotals.map(({ dateKey, total }) => {
                      const weekday = new Intl.DateTimeFormat("en-GB", {
                        timeZone: "Europe/Copenhagen",
                        weekday: "short",
                      }).format(new Date(`${dateKey}T12:00:00Z`));
                      return (
                        <div className="bar-column" key={dateKey} title={`${dateKey}: ${formatMillilitres(total)}`}>
                          <span className="bar-value">{total ? formatLitres(total) : ""}</span>
                          <div className="bar-track">
                            <div
                              className={`bar-fill ${dateKey === todayKey ? "bar-today" : ""}`}
                              style={{ height: `${Math.max((total / chartScale) * 100, total > 0 ? 5 : 0)}%` }}
                            />
                          </div>
                          <span className="bar-label">{weekday.slice(0, 1)}</span>
                        </div>
                      );
                    })}
                  </div>
                </section>
                <section className="history-list-section">
                  <h3>Daily totals</h3>
                  <div className="card daily-totals">
                    {weekTotals.map(({ dateKey, total }) => (
                      <div className="daily-total-row" key={dateKey}>
                        <span>{formatShortDate(dateKey)}</span>
                        <div className="daily-total-track">
                          <div
                            className="daily-total-fill"
                            style={{ width: `${Math.min((total / chartScale) * 100, 100)}%` }}
                          />
                        </div>
                        <strong>{formatMillilitres(total)}</strong>
                      </div>
                    ))}
                  </div>
                </section>
              </section>
            ) : (
              <section className="companion-page">
                <div className="companion-profile">
                  <p className="eyebrow">YOUR COMPANION</p>
                  <h2>{profileDisplayName(selectedProfile)}’s companion</h2>
                  <PetPortrait state={state} />
                  <h3>{state?.name ?? "Waiting for a status"}</h3>
                  <p className="muted">{state?.message ?? "The companion will appear when the device sends its first character update."}</p>
                  {pet && (
                    <p className="last-updated">
                      Last updated {formatTime(pet.time_received)} · {pet.pet_mood ?? state?.name}
                    </p>
                  )}
                </div>
                <div className="card companion-progress">
                  <div className="section-title-row">
                    <h3>Your water today</h3>
                    <span>{Math.round(progress)}%</span>
                  </div>
                  <div className="linear-progress">
                    <span style={{ width: `${progress}%` }} />
                  </div>
                  <div className="progress-labels">
                    <strong>{formatMillilitres(todayTotal)}</strong>
                    <span>of {formatMillilitres(target)}</span>
                  </div>
                </div>
              </section>
            )}
          </>
        )}
      </main>

      {selectedProfile && (
        <nav className="mobile-tabs" aria-label="App sections">
          <TabButton tab="overview" activeTab={activeTab} onSelect={setActiveTab} />
          <TabButton tab="history" activeTab={activeTab} onSelect={setActiveTab} />
          <TabButton tab="companion" activeTab={activeTab} onSelect={setActiveTab} />
        </nav>
      )}
    </div>
  );
}

function TabButton({
  tab,
  activeTab,
  onSelect,
}: {
  tab: Tab;
  activeTab: Tab;
  onSelect: (tab: Tab) => void;
}) {
  const labels: Record<Tab, string> = {
    overview: "Overview",
    history: "History",
    companion: "Companion",
  };
  const icons: Record<Tab, string> = {
    overview: "⌂",
    history: "▥",
    companion: "◉",
  };

  return (
    <button
      className={`tab-button ${activeTab === tab ? "tab-active" : ""}`}
      onClick={() => onSelect(tab)}
      type="button"
      aria-current={activeTab === tab ? "page" : undefined}
    >
      <span className="tab-icon" aria-hidden="true">{icons[tab]}</span>
      {labels[tab]}
    </button>
  );
}

function ActivityRow({ log }: { log: WaterLog }) {
  return (
    <div className="card activity-row">
      <span className="activity-icon" aria-hidden="true">◌</span>
      <div className="activity-description">
        <strong>Drank {formatMillilitres(Number(log.water_drank))}</strong>
        <span>Today at {formatTime(log.time_drank)}</span>
      </div>
      <span className="activity-chevron" aria-hidden="true">›</span>
    </div>
  );
}

function PetPortrait({ state }: { state: { image: string; name: string } | undefined }) {
  return (
    <div className="pet-portrait">
      <img
        src={state?.image ?? neutralImage}
        alt={state ? `${state.name} water companion` : "Water companion"}
      />
    </div>
  );
}

function CompanionCard({
  state,
  pet,
  target,
  total,
}: {
  state: { image: string; name: string; message: string } | undefined;
  pet: CharacterLog | null;
  target: number;
  total: number;
}) {
  const progress = target > 0 ? Math.min((total / target) * 100, 100) : 0;
  return (
    <aside className="companion-card card">
      <div className="section-title-row">
        <div>
          <p className="eyebrow">YOUR COMPANION</p>
          <h3>{state?.name ?? "Waiting for a status"}</h3>
        </div>
        <span className="status-dot" aria-label={pet ? "Status available" : "Waiting for status"} />
      </div>
      <PetPortrait state={state} />
      <p className="companion-message">
        {state?.message ?? "Your companion will appear after the first character update."}
      </p>
      {pet && <p className="last-updated">Updated at {formatTime(pet.time_received)}</p>}
      <div className="companion-meter">
        <div className="section-title-row">
          <span>Today</span>
          <strong>{formatMillilitres(total)} / {formatMillilitres(target)}</strong>
        </div>
        <div className="linear-progress">
          <span style={{ width: `${progress}%` }} />
        </div>
      </div>
    </aside>
  );
}

export default App;
