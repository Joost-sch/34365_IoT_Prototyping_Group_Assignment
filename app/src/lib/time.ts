const TIME_ZONE = "Europe/Copenhagen";

const datePartsFormatter = new Intl.DateTimeFormat("en-CA", {
  timeZone: TIME_ZONE,
  year: "numeric",
  month: "2-digit",
  day: "2-digit",
});

const dateTimePartsFormatter = new Intl.DateTimeFormat("en-GB", {
  timeZone: TIME_ZONE,
  year: "numeric",
  month: "2-digit",
  day: "2-digit",
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  hourCycle: "h23",
});

export type DateRange = {
  start: Date;
  end: Date;
};

function partsToDateKey(parts: Intl.DateTimeFormatPart[]): string {
  const values = Object.fromEntries(parts.map(({ type, value }) => [type, value]));
  return `${values.year}-${values.month}-${values.day}`;
}

export function copenhagenDateKey(date: Date): string {
  return partsToDateKey(datePartsFormatter.formatToParts(date));
}

function addCalendarDays(dateKey: string, days: number): string {
  const [year, month, day] = dateKey.split("-").map(Number);
  const date = new Date(Date.UTC(year, month - 1, day + days));
  return [
    date.getUTCFullYear(),
    String(date.getUTCMonth() + 1).padStart(2, "0"),
    String(date.getUTCDate()).padStart(2, "0"),
  ].join("-");
}

function copenhagenMidnight(dateKey: string): Date {
  const [year, month, day] = dateKey.split("-").map(Number);
  const utcGuess = Date.UTC(year, month - 1, day);
  const parts = Object.fromEntries(
    dateTimePartsFormatter
      .formatToParts(new Date(utcGuess))
      .map(({ type, value }) => [type, value]),
  );
  const localAsUtc = Date.UTC(
    Number(parts.year),
    Number(parts.month) - 1,
    Number(parts.day),
    Number(parts.hour),
    Number(parts.minute),
    Number(parts.second),
  );
  return new Date(utcGuess - (localAsUtc - utcGuess));
}

export function copenhagenDayRange(dateKey: string): DateRange {
  return {
    start: copenhagenMidnight(dateKey),
    end: copenhagenMidnight(addCalendarDays(dateKey, 1)),
  };
}

export function copenhagenWeek(date: Date, weekOffset = 0): {
  dateKeys: string[];
  range: DateRange;
} {
  const todayKey = copenhagenDateKey(date);
  const [year, month, day] = todayKey.split("-").map(Number);
  const dayOfWeek = new Date(Date.UTC(year, month - 1, day)).getUTCDay();
  const daysSinceMonday = (dayOfWeek + 6) % 7;
  const monday = addCalendarDays(todayKey, -daysSinceMonday + weekOffset * 7);
  const dateKeys = Array.from({ length: 7 }, (_, index) =>
    addCalendarDays(monday, index),
  );

  return {
    dateKeys,
    range: {
      start: copenhagenMidnight(monday),
      end: copenhagenMidnight(addCalendarDays(monday, 7)),
    },
  };
}

export function formatTime(dateString: string): string {
  return new Intl.DateTimeFormat("en-GB", {
    timeZone: TIME_ZONE,
    hour: "2-digit",
    minute: "2-digit",
  }).format(new Date(dateString));
}

export function formatShortDate(dateKey: string): string {
  const [year, month, day] = dateKey.split("-").map(Number);
  return new Intl.DateTimeFormat("en-GB", {
    timeZone: TIME_ZONE,
    day: "numeric",
    month: "short",
  }).format(new Date(Date.UTC(year, month - 1, day, 12)));
}

export function formatDateRange(startKey: string, endKey: string): string {
  const format = new Intl.DateTimeFormat("en-GB", {
    timeZone: TIME_ZONE,
    day: "numeric",
    month: "short",
    year: "numeric",
  });
  const dateFromKey = (dateKey: string) => {
    const [year, month, day] = dateKey.split("-").map(Number);
    return new Date(Date.UTC(year, month - 1, day, 12));
  };
  return `${format.format(dateFromKey(startKey))} – ${format.format(dateFromKey(endKey))}`;
}
