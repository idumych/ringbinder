/*
 * Ringbinder (web) - a tabbed personal organizer
 * Distributed under the terms of the MIT License.
 *
 * A port of the Haiku application. Records are plain objects with an "id";
 * all data lives in localStorage under one key.
 */
(function () {
'use strict';

// ---------------------------------------------------------------- constants

const SECTIONS = [
	{ key: 'calendar',    name: 'Calendar',    color: '#7aa0d6' },
	{ key: 'todo',        name: 'To Do',       color: '#eca25c' },
	{ key: 'address',     name: 'Address',     color: '#84be76' },
	{ key: 'calls',       name: 'Calls',       color: '#c684b6' },
	{ key: 'planner',     name: 'Planner',     color: '#e2ce60' },
	{ key: 'notepad',     name: 'Notepad',     color: '#8cc4c4' },
	{ key: 'anniversary', name: 'Anniversary', color: '#d67070' }
];
const CALENDAR = 0, TODO = 1, ADDRESS = 2, CALLS = 3, PLANNER = 4,
	NOTEPAD = 5, ANNIVERSARY = 6;

const STORAGE_KEY = 'ringbinder.data';
const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July',
	'August', 'September', 'October', 'November', 'December'];
const DAYS = ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday',
	'Saturday', 'Sunday'];

// ---------------------------------------------------------------- dates
// Dates are stored as ISO strings (YYYY-MM-DD) and handled as day numbers
// (days since 1970-01-01) so there is no timezone arithmetic.

const pad = (n) => String(n).padStart(2, '0');

function dayNumber(y, m, d) { return Math.round(Date.UTC(y, m - 1, d) / 864e5); }

function fromDayNumber(n) {
	const t = new Date(n * 864e5);
	return { y: t.getUTCFullYear(), m: t.getUTCMonth() + 1, d: t.getUTCDate() };
}

function parseDate(text) {
	const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(String(text || '').trim());
	if (!match) return null;
	const date = { y: +match[1], m: +match[2], d: +match[3] };
	const check = fromDayNumber(dayNumber(date.y, date.m, date.d));
	if (check.y !== date.y || check.m !== date.m || check.d !== date.d)
		return null;
	return date;
}

const iso = (date) => `${date.y}-${pad(date.m)}-${pad(date.d)}`;
const isoOfDay = (n) => iso(fromDayNumber(n));
const daysInMonth = (y, m) => new Date(Date.UTC(y, m, 0)).getUTCDate();
// 0 = Monday .. 6 = Sunday (1970-01-01 was a Thursday)
const weekday = (n) => (((n + 3) % 7) + 7) % 7;

function today() {
	const now = new Date();
	return { y: now.getFullYear(), m: now.getMonth() + 1, d: now.getDate() };
}
const todayNumber = () => { const t = today(); return dayNumber(t.y, t.m, t.d); };

function formatShort(text) {
	const d = parseDate(text);
	return d ? `${d.d} ${MONTHS[d.m - 1].slice(0, 3)} ${d.y}` : text || '';
}

function formatLong(text) {
	const d = parseDate(text);
	if (!d) return text || '';
	return `${DAYS[weekday(dayNumber(d.y, d.m, d.d))]}, ${d.d} ${MONTHS[d.m - 1]} ${d.y}`;
}

// Days until the next yearly occurrence of a date (0 = today)
function daysUntilNext(date) {
	const now = todayNumber(), year = today().y;
	for (let y = year; y <= year + 8; y++) {
		let day = date.d;
		if (date.m === 2 && day === 29 && daysInMonth(y, 2) !== 29) day = 28;
		const diff = dayNumber(y, date.m, day) - now;
		if (diff >= 0) return diff;
	}
	return 0;
}

function nowTime() {
	const now = new Date();
	return `${pad(now.getHours())}:${pad(now.getMinutes())}`;
}

// ---------------------------------------------------------------- data store

const store = {
	data: { records: {}, nextId: 1, section: CALENDAR },
	persistent: true,

	load() {
		let raw = null;
		try { raw = localStorage.getItem(STORAGE_KEY); } catch (e) { this.persistent = false; }
		if (raw) {
			try {
				this.adopt(JSON.parse(raw));
				return;
			} catch (e) {
				// keep the unreadable text aside rather than overwrite it
				try { localStorage.setItem(STORAGE_KEY + '.unreadable', raw); } catch (e2) { /* ignore */ }
			}
		}
		this.seed();
	},

	adopt(data) {
		const records = {};
		let nextId = 1;
		SECTIONS.forEach((s) => {
			const list = data && data.records && Array.isArray(data.records[s.key])
				? data.records[s.key] : [];
			records[s.key] = list.filter((r) => r && typeof r === 'object');
			records[s.key].forEach((r) => { if (r.id >= nextId) nextId = r.id + 1; });
		});
		const section = Number.isInteger(data.section) && data.section >= 0
			&& data.section < SECTIONS.length ? data.section : CALENDAR;
		this.data = { records, nextId, section };
	},

	seed() {
		this.adopt({});
		this.put('notepad', {
			title: 'Welcome to Ringbinder',
			body: 'Ringbinder keeps your diary, tasks, contacts, calls, year planner, '
				+ 'notes and anniversaries in one binder.\n\n'
				+ '• Click a coloured tab to turn to its section, or press 1 … 7. '
				+ 'Like the dividers in a real binder, the tabs of the sections you '
				+ 'have turned past move to the left-hand edge.\n'
				+ '• Double-click an empty day in the Calendar to add an '
				+ 'appointment; double-click an entry to edit it.\n'
				+ '• In the Planner, drag across days to block out a vacation or trip.\n'
				+ '• Anniversaries and due To Do items show up on the Calendar '
				+ 'automatically.\n\n'
				+ 'Everything is saved as you go, in this browser. Use Export to keep '
				+ 'a copy of your binder, or to move it to another browser.'
		});
	},

	save() {
		const status = document.getElementById('saveStatus');
		try {
			localStorage.setItem(STORAGE_KEY, JSON.stringify(this.data));
			status.textContent = '';
			this.persistent = true;
		} catch (e) {
			this.persistent = false;
			status.textContent = 'Not saved – browser storage is unavailable. Export to keep your data.';
		}
	},

	list(section) { return this.data.records[section]; },
	find(section, id) { return this.list(section).find((r) => r.id === id) || null; },

	put(section, record) {
		const list = this.list(section);
		const copy = Object.assign({}, record);
		if (!copy.id) copy.id = this.data.nextId++;
		copy.modified = Math.floor(Date.now() / 1000);
		const at = list.findIndex((r) => r.id === copy.id);
		if (at >= 0) list[at] = copy; else list.push(copy);
		this.save();
		return copy.id;
	},

	remove(section, id) {
		const list = this.list(section);
		const at = list.findIndex((r) => r.id === id);
		if (at >= 0) list.splice(at, 1);
		this.save();
	},

	exportJson() {
		return JSON.stringify({ app: 'ringbinder', version: 1, exported: new Date().toISOString(),
			records: this.data.records }, null, 2);
	},

	importJson(text) {
		const parsed = JSON.parse(text);
		if (!parsed || parsed.app !== 'ringbinder' || !parsed.records)
			throw new Error('This is not a Ringbinder export.');
		this.adopt({ records: parsed.records, section: this.data.section });
		this.save();
	}
};

// ---------------------------------------------------------------- helpers

const esc = (s) => String(s == null ? '' : s).replace(/[&<>"']/g,
	(c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

const $ = (sel, root) => (root || document).querySelector(sel);
const $$ = (sel, root) => Array.from((root || document).querySelectorAll(sel));
const collator = new Intl.Collator(undefined, { sensitivity: 'base', numeric: false });
const getStr = (r, k) => (r && r[k] != null ? String(r[k]) : '');

// ---------------------------------------------------------------- editor dialog

const editor = $('#editor'), editorForm = $('#editorForm');

function openEditor(title, fields, record, onSave) {
	const rows = fields.map((f, i) => {
		const value = record[f.key];
		const id = 'f' + i;
		let control;
		switch (f.type) {
			case 'multiline':
				control = `<textarea id="${id}" name="${f.key}" rows="4">${esc(value)}</textarea>`;
				break;
			case 'date':
				control = `<input id="${id}" name="${f.key}" type="date" value="${esc(value)}">`;
				break;
			case 'time':
				control = `<input id="${id}" name="${f.key}" type="time" value="${esc(value)}">`;
				break;
			case 'check':
				control = `<input id="${id}" name="${f.key}" type="checkbox"${value ? ' checked' : ''}>`;
				break;
			case 'choice': {
				const choices = f.choices.slice();
				if (value && !choices.includes(value)) choices.push(value);
				control = `<select id="${id}" name="${f.key}">${choices.map((c) =>
					`<option${c === (value || choices[0]) ? ' selected' : ''}>${esc(c)}</option>`).join('')}</select>`;
				break;
			}
			default:
				control = `<input id="${id}" name="${f.key}" type="text" value="${esc(value)}"${f.required ? ' required' : ''}>`;
		}
		return `<label for="${id}">${esc(f.label)}${f.required ? ' *' : ''}</label>${control}`;
	}).join('');

	editorForm.innerHTML = `<h2>${esc(title)}</h2><div class="fields">${rows}</div>
		<p class="error" role="alert"></p>
		<menu><button type="button" data-cancel>Cancel</button><button class="primary" value="ok">Save</button></menu>`;

	const error = $('.error', editorForm);
	editorForm.onsubmit = (event) => {
		event.preventDefault();
		const result = Object.assign({}, record);
		for (let i = 0; i < fields.length; i++) {
			const f = fields[i], el = $('#f' + i, editorForm);
			if (f.type === 'check') result[f.key] = el.checked;
			else result[f.key] = el.value.trim();
			if (f.required && !result[f.key]) {
				error.textContent = `${f.label} is required.`;
				el.focus();
				return;
			}
		}
		if (result.start && result.end && fields.some((f) => f.type === 'date')
			&& parseDate(result.start) && parseDate(result.end) && result.end < result.start) {
			const swap = result.start; result.start = result.end; result.end = swap;
		}
		editor.close();
		onSave(result);
	};
	$('[data-cancel]', editorForm).onclick = () => editor.close();
	editor.showModal();
	const first = $('input[type=text], input[type=date], textarea', editorForm);
	if (first) first.focus();
}

function anyDialogOpen() { return $$('dialog').some((d) => d.open); }

// ---------------------------------------------------------------- app state

const view = {
	section: CALENDAR,
	calendar: { monday: 0, selDate: 0, selId: -1 },
	list: { todo: { tab: 0, sel: -1 }, address: { tab: 0, sel: -1 }, calls: { tab: 0, sel: -1 } },
	planner: { year: today().y, selId: -1 },
	notepad: { sel: -1 },
	anniversary: { year: today().y, firstMonth: today().m <= 6 ? 1 : 7, sel: -1 }
};

const pageEl = $('#page');
let current = null;			// the active section object

function render() {
	const section = SECTIONS[view.section];
	pageEl.style.setProperty('--accent', section.color);
	pageEl.className = view.section === PLANNER ? 'single' : 'spreadpage';
	renderTabs();
	current = sections[view.section];
	current.render();
}

function refresh() { render(); }

function turnTo(index) {
	if (index < 0 || index >= SECTIONS.length) return;
	if (current && current.commit) current.commit();
	view.section = index;
	store.data.section = index;
	store.save();
	render();
}

function renderTabs() {
	const tab = (s, i) => `<button type="button" class="tab${i === view.section ? ' selected' : ''}" `
		+ `style="--slot:${i};--tab:${s.color}" data-section="${i}" title="${esc(s.name)} (${i + 1})">`
		+ `<span>${esc(s.name)}</span></button>`;
	$('#tabsLeft').innerHTML = SECTIONS.map((s, i) => i <= view.section ? tab(s, i) : '').join('');
	$('#tabsRight').innerHTML = SECTIONS.map((s, i) => i > view.section ? tab(s, i) : '').join('');
}

function pageHeader(title) {
	const t = today();
	return `<div class="header"><h1>${esc(title)}</h1><span class="date">${esc(formatLong(iso(t)))}</span></div>`;
}

function button(label, act, extra) {
	return `<button type="button" data-act="${act}"${extra || ''}>${label}</button>`;
}

const RINGS = 6;
function rings() {
	let out = '';
	for (let i = 0; i < RINGS; i++)
		out += `<i class="ring" style="--n:${i}"></i>`;
	return out;
}

function spread(left, right) {
	return `<div class="spread"><section class="leaf left">${left}</section>`
		+ `<section class="leaf right">${right}</section>${rings()}</div>`;
}

// ---------------------------------------------------------------- Calendar

const APPOINTMENT_FIELDS = [
	{ key: 'title', label: 'Description', type: 'text', required: true },
	{ key: 'date', label: 'Date', type: 'date', required: true },
	{ key: 'start', label: 'Start time', type: 'time' },
	{ key: 'end', label: 'End time', type: 'time' },
	{ key: 'location', label: 'Location', type: 'text' },
	{ key: 'category', label: 'Category', type: 'choice',
		choices: ['Business', 'Personal', 'Travel', 'Holiday', 'Other'] },
	{ key: 'notes', label: 'Notes', type: 'multiline' }
];

function dayEntries(date) {
	const out = [], text = iso(date);
	store.list('calendar').forEach((r) => {
		if (r.date !== text) return;
		let time = getStr(r, 'start');
		if (getStr(r, 'end')) time += '–' + r.end;
		let label = getStr(r, 'title');
		if (getStr(r, 'location')) label += ` (${r.location})`;
		out.push({ kind: 0, key: getStr(r, 'start'), id: r.id, time, text: label });
	});
	store.list('anniversary').forEach((r) => {
		const when = parseDate(r.date);
		if (!when || when.m !== date.m || when.d !== date.d) return;
		const years = date.y - when.y;
		out.push({ kind: 1, key: '', id: -1,
			text: '★ ' + getStr(r, 'title') + (years > 0 ? ` (${years})` : '') });
	});
	store.list('todo').forEach((r) => {
		if (r.done || r.due !== text) return;
		out.push({ kind: 2, key: getStr(r, 'priority'), id: -1, text: '☐ ' + getStr(r, 'title') });
	});
	out.sort((a, b) => a.kind - b.kind || (a.key < b.key ? -1 : a.key > b.key ? 1 : 0));
	return out;
}

function weekLabel(monday) {
	const first = fromDayNumber(monday), last = fromDayNumber(monday + 6);
	if (first.m === last.m)
		return `${first.d} – ${last.d} ${MONTHS[last.m - 1]} ${last.y}`;
	if (first.y === last.y)
		return `${first.d} ${MONTHS[first.m - 1]} – ${last.d} ${MONTHS[last.m - 1]} ${last.y}`;
	return `${first.d} ${MONTHS[first.m - 1]} ${first.y} – ${last.d} ${MONTHS[last.m - 1]} ${last.y}`;
}

const calendar = {
	render() {
		const s = view.calendar;
		if (!s.monday) {
			s.monday = todayNumber() - weekday(todayNumber());
			s.selDate = todayNumber();
		}
		const day = (offset, span) => {
			const n = s.monday + offset, date = fromDayNumber(n);
			const classes = ['day', n === todayNumber() ? 'today' : '', n === s.selDate ? 'selected' : ''];
			const entries = dayEntries(date).map((e) => {
				if (e.kind === 0) {
					return `<div class="line appt${e.id === s.selId ? ' sel' : ''}" data-id="${e.id}" data-date="${n}">`
						+ `<span class="time">${esc(e.time)}</span><span class="txt">${esc(e.text)}</span></div>`;
				}
				return `<div class="line ${e.kind === 1 ? 'anniv' : 'todo'}"><span class="txt">${esc(e.text)}</span></div>`;
			}).join('');
			return `<div class="${classes.join(' ')}" data-date="${n}" style="grid-row:span ${span}">`
				+ `<div class="dayhead">${DAYS[offset]} ${date.d} ${MONTHS[date.m - 1]}</div>`
				+ `<div class="daybody">${entries}</div></div>`;
		};
		const leftDays = [0, 1, 2].map((i) => day(i, 2)).join('');
		const rightDays = day(3, 2) + day(4, 2) + day(5, 1) + day(6, 1);

		pageEl.innerHTML = pageHeader('Calendar')
			+ `<div class="toolbar">${button('◀ Previous week', 'prev')}${button('This week', 'today')}`
			+ `${button('Next week ▶', 'next')}<strong class="label">${esc(weekLabel(s.monday))}</strong>`
			+ `<span class="grow"></span>${button('New appointment', 'new')}${button('Edit', 'edit')}${button('Delete', 'delete')}</div>`
			+ `<div class="body week">${spread(`<div class="days">${leftDays}</div>`, `<div class="days">${rightDays}</div>`)}</div>`;

		$$('.day', pageEl).forEach((el) => {
			el.addEventListener('click', (ev) => {
				const line = ev.target.closest('.appt');
				s.selDate = +el.dataset.date;
				s.selId = line ? +line.dataset.id : -1;
				refresh();
			});
			el.addEventListener('dblclick', (ev) => {
				const line = ev.target.closest('.appt');
				s.selDate = +el.dataset.date;
				if (line) { s.selId = +line.dataset.id; this.act('edit'); }
				else { s.selId = -1; this.newOn(s.selDate); }
			});
		});
	},

	act(name) {
		const s = view.calendar;
		switch (name) {
			case 'prev': s.monday -= 7; s.selDate -= 7; s.selId = -1; refresh(); break;
			case 'next': s.monday += 7; s.selDate += 7; s.selId = -1; refresh(); break;
			case 'today':
				s.monday = todayNumber() - weekday(todayNumber());
				s.selDate = todayNumber(); s.selId = -1; refresh(); break;
			case 'new': this.newOn(s.selDate); break;
			case 'edit': {
				const record = store.find('calendar', s.selId);
				if (record) openEditor('Edit appointment', APPOINTMENT_FIELDS, record, (r) => {
					store.put('calendar', r); refresh();
				});
				break;
			}
			case 'delete': {
				const record = store.find('calendar', s.selId);
				if (record && confirm(`Delete "${getStr(record, 'title')}"?`)) {
					store.remove('calendar', record.id); s.selId = -1; refresh();
				}
				break;
			}
		}
	},

	newOn(dayNum) {
		const record = { date: isoOfDay(dayNum), start: '09:00', end: '10:00' };
		openEditor('New appointment', APPOINTMENT_FIELDS, record, (r) => {
			view.calendar.selId = store.put('calendar', r);
			view.calendar.selDate = dayNumber(...Object.values(parseDate(r.date)));
			refresh();
		});
	}
};

// ---------------------------------------------------------------- list sections (To Do, Address, Calls)

const LISTS = {
	todo: {
		index: TODO, singular: 'task', newLabel: 'New Task', mode: 'todo',
		fields: [
			{ key: 'title', label: 'Task', type: 'text', required: true },
			{ key: 'priority', label: 'Priority', type: 'choice', choices: ['2 - Normal', '1 - High', '3 - Low'] },
			{ key: 'due', label: 'Due date', type: 'date' },
			{ key: 'category', label: 'Category', type: 'choice',
				choices: ['Business', 'Personal', 'Home', 'Errands', 'Other'] },
			{ key: 'done', label: 'Completed', type: 'check' },
			{ key: 'notes', label: 'Notes', type: 'multiline' }
		],
		columns: [
			{ key: 'done', title: '', width: 24, fmt: 'check' },
			{ key: 'priority', title: 'Pri', width: 34, fmt: 'short' },
			{ key: 'title', title: 'Task', width: 0, fmt: 'text' },
			{ key: 'due', title: 'Due', width: 96, fmt: 'date' }
		],
		sortKeys: ['priority', 'due', 'title'], doneKey: 'done',
		cardTitle: ['title'], tabs: ['All', 'Overdue', 'Current', 'Future', 'Completed']
	},
	address: {
		index: ADDRESS, singular: 'contact', newLabel: 'New Contact', mode: 'letters',
		fields: [
			{ key: 'last', label: 'Last name', type: 'text' },
			{ key: 'first', label: 'First name', type: 'text' },
			{ key: 'company', label: 'Company', type: 'text' },
			{ key: 'jobtitle', label: 'Job title', type: 'text' },
			{ key: 'phone', label: 'Phone', type: 'text' },
			{ key: 'mobile', label: 'Mobile', type: 'text' },
			{ key: 'email', label: 'E-mail', type: 'text' },
			{ key: 'address', label: 'Address', type: 'multiline' },
			{ key: 'birthday', label: 'Birthday', type: 'date' },
			{ key: 'notes', label: 'Notes', type: 'multiline' }
		],
		columns: [
			{ key: 'last', title: 'Last name', width: 0, fmt: 'text' },
			{ key: 'first', title: 'First name', width: 110, fmt: 'text' },
			{ key: 'phone', title: 'Phone', width: 110, fmt: 'text' }
		],
		sortKeys: ['last', 'first', 'company'], doneKey: null, letterKey: 'last',
		cardTitle: ['first', 'last'],
		tabs: ['All'].concat('ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split(''))
	},
	calls: {
		index: CALLS, singular: 'call', newLabel: 'New Call', mode: 'none',
		fields: [
			{ key: 'name', label: 'Name', type: 'text', required: true },
			{ key: 'company', label: 'Company', type: 'text' },
			{ key: 'phone', label: 'Phone', type: 'text' },
			{ key: 'direction', label: 'Type', type: 'choice', choices: ['Outgoing', 'Incoming', 'Missed', 'Call back'] },
			{ key: 'date', label: 'Date', type: 'date' },
			{ key: 'time', label: 'Time', type: 'time' },
			{ key: 'done', label: 'Completed', type: 'check' },
			{ key: 'notes', label: 'Notes', type: 'multiline' }
		],
		columns: [
			{ key: 'done', title: '', width: 24, fmt: 'check' },
			{ key: 'date', title: 'Date', width: 96, fmt: 'date' },
			{ key: 'time', title: 'Time', width: 50, fmt: 'text' },
			{ key: 'name', title: 'Name', width: 0, fmt: 'text' },
			{ key: 'direction', title: 'Type', width: 76, fmt: 'text' }
		],
		sortKeys: ['date', 'time', 'name'], doneKey: 'done',
		cardTitle: ['name'], tabs: null,
		defaults: () => ({ date: iso(today()), time: nowTime() })
	}
};
const FUTURE_DAYS = 7;

function matches(config, record, tab) {
	if (tab === 0) return true;
	if (config.mode === 'letters') {
		const value = getStr(record, config.letterKey) || getStr(record, 'company');
		return value !== '' && value[0].toUpperCase() === config.tabs[tab];
	}
	if (config.mode === 'todo') {
		const done = !!record.done;
		if (tab === 4) return done;
		if (done) return false;
		const due = parseDate(record.due);
		const now = todayNumber();
		const n = due ? dayNumber(due.y, due.m, due.d) : null;
		const overdue = n !== null && n < now;
		const future = n !== null && n > now + FUTURE_DAYS;
		if (tab === 1) return overdue;
		if (tab === 2) return !overdue && !future;
		if (tab === 3) return future;
	}
	return true;
}

function sortRecords(config, records) {
	return records.slice().sort((a, b) => {
		if (config.doneKey && !!a[config.doneKey] !== !!b[config.doneKey])
			return a[config.doneKey] ? 1 : -1;
		for (const key of config.sortKeys) {
			const x = getStr(a, key), y = getStr(b, key);
			if ((x === '') !== (y === '')) return x === '' ? 1 : -1;
			const c = collator.compare(x, y);
			if (c !== 0) return c;
		}
		return a.id - b.id;
	});
}

function cellText(config, column, record) {
	const value = getStr(record, column.key);
	if (column.fmt === 'date') return formatShort(value);
	if (column.fmt === 'short') return value.split(' - ')[0];
	return value;
}

function cardHtml(config, record) {
	const titleKeys = config.cardTitle;
	const heading = titleKeys.map((k) => getStr(record, k)).filter(Boolean).join(' ')
		|| getStr(record, 'company') || '(untitled)';
	const rows = config.fields.filter((f) => !titleKeys.includes(f.key)).map((f) => {
		if (f.type === 'check') return record[f.key] ? `<dt>${esc(f.label)}</dt><dd>Yes</dd>` : '';
		const value = getStr(record, f.key);
		if (!value) return '';
		const text = f.type === 'date' ? formatLong(value) : value;
		return `<dt>${esc(f.label)}</dt><dd${f.type === 'multiline' ? ' class="pre"' : ''}>${esc(text)}</dd>`;
	}).join('');
	return `<div class="card" title="Double-click to edit"><h2>${esc(heading)}</h2><dl>${rows}</dl></div>`
		+ '<p class="hint">Double-click the card to edit</p>';
}

function makeListSection(key) {
	const config = LISTS[key];
	const state = view.list[key];
	const section = {
		visible() {
			return sortRecords(config, store.list(key).filter((r) => matches(config, r, state.tab)));
		},

		render() {
			const visible = this.visible();
			if (!visible.some((r) => r.id === state.sel))
				state.sel = visible.length ? visible[0].id : -1;
			const selected = store.find(key, state.sel);

			const template = config.columns.map((c) => c.width ? `${c.width}px` : 'minmax(0,1fr)').join(' ');
			const head = config.columns.map((c) => `<span>${esc(c.title)}</span>`).join('');
			const rows = visible.map((r) => {
				const done = config.doneKey && r[config.doneKey];
				const cells = config.columns.map((c) => c.fmt === 'check'
					? `<span class="check">${r[c.key] ? '☑' : '☐'}</span>`
					: `<span>${esc(cellText(config, c, r))}</span>`).join('');
				return `<div class="row${r.id === state.sel ? ' sel' : ''}${done ? ' done' : ''}" `
					+ `data-id="${r.id}" style="grid-template-columns:${template}">${cells}</div>`;
			}).join('');

			let tabs = '';
			if (config.tabs) {
				tabs = `<div class="indextabs ${config.mode}">` + config.tabs.map((label, i) =>
					`<button type="button" class="${i === state.tab ? 'on' : ''}" data-tab="${i}">`
					+ `<span>${esc(label)}</span></button>`).join('') + '</div>';
			}
			const count = `${visible.length} ${config.singular}${visible.length === 1 ? '' : 's'}`;
			const toggle = config.doneKey
				? button(selected && selected[config.doneKey] ? 'Mark not done' : 'Mark done', 'toggle') : '';

			const list = `<div class="listwrap">${tabs}<div class="list"><div class="listhead" style="grid-template-columns:${template}">${head}</div>`
				+ `<div class="rows">${rows || '<p class="empty">Nothing here.</p>'}</div></div></div>`;
			const card = selected ? cardHtml(config, selected) : '<p class="empty">No entry selected.</p>';

			pageEl.innerHTML = pageHeader(SECTIONS[config.index].name)
				+ `<div class="toolbar">${button(esc(config.newLabel), 'new')}${button('Edit', 'edit')}`
				+ `${button('Delete', 'delete')}${toggle}<span class="grow"></span><span class="count">${esc(count)}</span></div>`
				+ `<div class="body">${spread(list, `<div class="cardpage">${card}</div>`)}</div>`;

			$$('.row', pageEl).forEach((el) => {
				el.addEventListener('click', () => { state.sel = +el.dataset.id; refresh(); });
				el.addEventListener('dblclick', () => { state.sel = +el.dataset.id; this.act('edit'); });
			});
			$$('[data-tab]', pageEl).forEach((el) => el.addEventListener('click', () => {
				state.tab = +el.dataset.tab; refresh();
			}));
			const cardEl = $('.card', pageEl);
			if (cardEl) cardEl.addEventListener('dblclick', () => this.act('edit'));
		},

		act(name) {
			const selected = store.find(key, state.sel);
			switch (name) {
				case 'new': {
					const record = config.defaults ? config.defaults() : {};
					openEditor(config.newLabel, config.fields, record, (r) => {
						state.sel = store.put(key, r);
						// make sure the new entry isn't hidden by the current tab
						if (!matches(config, r, state.tab)) state.tab = 0;
						refresh();
					});
					break;
				}
				case 'edit':
					if (selected) openEditor('Edit ' + config.singular, config.fields, selected, (r) => {
						store.put(key, r); refresh();
					});
					break;
				case 'delete': {
					const label = config.cardTitle.map((k) => getStr(selected, k)).filter(Boolean).join(' ');
					if (selected && confirm(`Delete "${label}"?`)) {
						store.remove(key, selected.id); state.sel = -1; refresh();
					}
					break;
				}
				case 'toggle':
					if (selected && config.doneKey) {
						store.put(key, Object.assign({}, selected, { [config.doneKey]: !selected[config.doneKey] }));
						refresh();
					}
					break;
			}
		}
	};
	return section;
}

// ---------------------------------------------------------------- Planner

const PLANNER_CATEGORIES = ['Meeting', 'Vacation', 'Travel', 'Training', 'Holiday', 'Other'];
const PLANNER_COLORS = ['#6e96dc', '#78be6e', '#e6a050', '#b478c8', '#dc6464', '#a0a0a0'];
const PLANNER_FIELDS = [
	{ key: 'title', label: 'Event', type: 'text', required: true },
	{ key: 'category', label: 'Category', type: 'choice', choices: PLANNER_CATEGORIES },
	{ key: 'start', label: 'Start date', type: 'date', required: true },
	{ key: 'end', label: 'End date', type: 'date', required: true },
	{ key: 'notes', label: 'Notes', type: 'multiline' }
];
const COLUMNS = 37;

function eventRange(record) {
	const a = parseDate(record.start);
	if (!a) return null;
	const b = parseDate(record.end) || a;
	let start = dayNumber(a.y, a.m, a.d), end = dayNumber(b.y, b.m, b.d);
	if (end < start) [start, end] = [end, start];
	return { start, end };
}

const planner = {
	render() {
		const s = view.planner;
		const now = today();
		const events = store.list('planner').map((r) => ({ r, range: eventRange(r) })).filter((e) => e.range);

		const letters = Array.from({ length: COLUMNS }, (_, c) =>
			`<span class="${c % 7 >= 5 ? 'we' : ''}">${DAYS[c % 7][0]}</span>`).join('');

		let rows = '';
		for (let m = 1; m <= 12; m++) {
			const first = dayNumber(s.year, m, 1);
			const offset = weekday(first), days = daysInMonth(s.year, m);
			let cells = '', bars = '';
			for (let d = 1; d <= days; d++) {
				const n = first + d - 1, col = offset + d;
				const isToday = s.year === now.y && m === now.m && d === now.d;
				cells += `<i class="cell${(col - 1) % 7 >= 5 ? ' we' : ''}${d === 1 ? ' first' : ''}${isToday ? ' today' : ''}" `
					+ `style="grid-column:${col}" data-day="${n}"><b>${d}</b></i>`;
			}
			events.forEach(({ r, range }) => {
				const from = Math.max(range.start, first), to = Math.min(range.end, first + days - 1);
				if (from > to) return;
				const category = PLANNER_CATEGORIES.indexOf(r.category);
				const color = PLANNER_COLORS[category < 0 ? PLANNER_COLORS.length - 1 : category];
				bars += `<span class="bar${r.id === s.selId ? ' sel' : ''}" style="grid-column:${offset + (from - first) + 1}/span ${to - from + 1};background:${color}">`
					+ `${esc(getStr(r, 'title'))}</span>`;
			});
			rows += `<div class="month"><span class="mname">${MONTHS[m - 1]}</span><div class="grid">${cells}${bars}</div></div>`;
		}
		const legend = PLANNER_CATEGORIES.map((c, i) =>
			`<span><i style="background:${PLANNER_COLORS[i]}"></i>${c}</span>`).join('');

		pageEl.innerHTML = pageHeader('Planner')
			+ `<div class="toolbar">${button('◀', 'prev')}<strong class="label year">${s.year}</strong>${button('▶', 'next')}`
			+ `${button('This year', 'today')}<span class="grow"></span>${button('New event', 'new')}${button('Edit', 'edit')}${button('Delete', 'delete')}</div>`
			+ `<div class="body"><div class="year"><div class="margin">${rings()}</div><div class="plan">`
			+ `<div class="weekdays"><span class="mname"></span><div class="grid">${letters}</div></div>${rows}`
			+ `<div class="legend">${legend}<em>Drag across days to add an event</em></div></div></div></div>`;

		this.bind();
	},

	eventAt(day) {
		let found = -1;
		store.list('planner').forEach((r) => {
			const range = eventRange(r);
			if (range && day >= range.start && day <= range.end) found = r.id;
		});
		return found;
	},

	bind() {
		const s = view.planner;
		const plan = $('.plan', pageEl);
		let dragStart = null, dragEnd = null;

		const paint = () => {
			const lo = Math.min(dragStart, dragEnd), hi = Math.max(dragStart, dragEnd);
			$$('.cell', plan).forEach((c) => c.classList.toggle('drag',
				dragStart !== null && +c.dataset.day >= lo && +c.dataset.day <= hi));
		};
		const cellOf = (target) => target.closest ? target.closest('.cell') : null;

		plan.addEventListener('mousedown', (ev) => {
			const cell = cellOf(ev.target);
			if (!cell || ev.button !== 0) return;
			dragStart = dragEnd = +cell.dataset.day;
			ev.preventDefault();
		});
		plan.addEventListener('mouseover', (ev) => {
			const cell = cellOf(ev.target);
			if (dragStart === null || !cell) return;
			dragEnd = +cell.dataset.day;
			paint();
		});
		const finish = () => {
			if (dragStart === null) return;
			const a = dragStart, b = dragEnd;
			dragStart = dragEnd = null;
			document.removeEventListener('mouseup', finish);
			if (a !== b) {
				const lo = Math.min(a, b), hi = Math.max(a, b);
				this.newEvent(isoOfDay(lo), isoOfDay(hi));
			} else {
				paint();
			}
		};
		plan.addEventListener('mousedown', () => document.addEventListener('mouseup', finish));

		plan.addEventListener('click', (ev) => {
			const cell = cellOf(ev.target);
			if (!cell) return;
			const id = this.eventAt(+cell.dataset.day);
			if (id !== s.selId) { s.selId = id; refresh(); }
		});
		plan.addEventListener('dblclick', (ev) => {
			const cell = cellOf(ev.target);
			if (!cell) return;
			const id = this.eventAt(+cell.dataset.day);
			if (id >= 0) { s.selId = id; this.act('edit'); }
		});
	},

	newEvent(start, end) {
		const date = iso({ y: view.planner.year, m: 1, d: 1 });
		openEditor('New event', PLANNER_FIELDS,
			{ start: start || date, end: end || start || date, category: 'Meeting' }, (r) => {
				view.planner.selId = store.put('planner', r); refresh();
			});
	},

	act(name) {
		const s = view.planner;
		switch (name) {
			case 'prev': s.year--; s.selId = -1; refresh(); break;
			case 'next': s.year++; s.selId = -1; refresh(); break;
			case 'today': s.year = today().y; s.selId = -1; refresh(); break;
			case 'new': this.newEvent(); break;
			case 'edit': {
				const record = store.find('planner', s.selId);
				if (record) openEditor('Edit event', PLANNER_FIELDS, record, (r) => { store.put('planner', r); refresh(); });
				break;
			}
			case 'delete': {
				const record = store.find('planner', s.selId);
				if (record && confirm(`Delete "${getStr(record, 'title')}"?`)) {
					store.remove('planner', record.id); s.selId = -1; refresh();
				}
				break;
			}
		}
	}
};

// ---------------------------------------------------------------- Notepad

const notepad = {
	pages() {
		return store.list('notepad').slice().sort((a, b) =>
			collator.compare(getStr(a, 'title'), getStr(b, 'title')) || a.id - b.id);
	},

	render(focusTitle) {
		const s = view.notepad;
		const pages = this.pages();
		if (!pages.some((p) => p.id === s.sel)) s.sel = pages.length ? pages[0].id : -1;
		const page = store.find('notepad', s.sel);

		const contents = pages.map((p) => `<div class="row${p.id === s.sel ? ' sel' : ''}" data-id="${p.id}">`
			+ `<span>${esc(getStr(p, 'title') || '(untitled)')}</span></div>`).join('');
		const left = `<div class="listwrap"><div class="list"><div class="listhead"><span>Contents</span></div>`
			+ `<div class="rows" id="contents">${contents || '<p class="empty">No pages.</p>'}</div></div></div>`;
		const right = page
			? `<div class="notepage"><input id="noteTitle" type="text" placeholder="Title" value="${esc(getStr(page, 'title'))}" aria-label="Title">`
				+ `<textarea id="noteBody" aria-label="Note">${esc(getStr(page, 'body'))}</textarea></div>`
			: '<p class="empty">No page open.</p>';

		pageEl.innerHTML = pageHeader('Notepad')
			+ `<div class="toolbar">${button('New page', 'new')}${button('Delete', 'delete')}<span class="grow"></span>`
			+ `<span class="count">${pages.length} page${pages.length === 1 ? '' : 's'}</span></div>`
			+ `<div class="body">${spread(left, right)}</div>`;

		$$('.row', pageEl).forEach((el) => el.addEventListener('click', () => {
			this.commit(); s.sel = +el.dataset.id; refresh();
		}));
		const title = $('#noteTitle'), body = $('#noteBody');
		if (title) {
			const save = () => {
				const record = store.find('notepad', s.sel);
				if (!record) return;
				store.put('notepad', Object.assign({}, record, { title: title.value, body: body.value }));
				const item = $(`.row[data-id="${s.sel}"] span`, pageEl);
				if (item) item.textContent = title.value || '(untitled)';
			};
			title.addEventListener('input', save);
			body.addEventListener('input', save);
			title.addEventListener('blur', () => this.commit());
			if (focusTitle) { title.focus(); title.select(); }
		}
	},

	// Edits are saved as they are typed; this re-sorts the contents
	commit() {
		const contents = $('#contents');
		if (contents && view.section === NOTEPAD) {
			const sorted = this.pages().map((p) => p.id).join();
			const shown = $$('.row', contents).map((r) => r.dataset.id).join();
			if (sorted !== shown) refresh();
		}
	},

	act(name) {
		const s = view.notepad;
		if (name === 'new') {
			s.sel = store.put('notepad', { title: 'Untitled', body: '' });
			this.render(true);
		} else if (name === 'delete') {
			const record = store.find('notepad', s.sel);
			if (record && confirm(`Delete "${getStr(record, 'title') || 'this page'}"?`)) {
				store.remove('notepad', record.id); s.sel = -1; refresh();
			}
		}
	}
};

// ---------------------------------------------------------------- Anniversary

const ANNIVERSARY_FIELDS = [
	{ key: 'title', label: 'Occasion', type: 'text', required: true },
	{ key: 'date', label: 'Date', type: 'date', required: true },
	{ key: 'category', label: 'Category', type: 'choice', choices: ['Birthday', 'Anniversary', 'Holiday', 'Other'] },
	{ key: 'notes', label: 'Notes', type: 'multiline' }
];

const anniversary = {
	monthBox(month, rightPage) {
		const s = view.anniversary, now = today();
		const occasions = store.list('anniversary').map((r) => ({ r, date: parseDate(r.date) }))
			.filter((o) => o.date && o.date.m === month)
			.sort((a, b) => a.date.d - b.date.d || collator.compare(getStr(a.r, 'title'), getStr(b.r, 'title')));
		const lines = occasions.map(({ r, date }) => {
			const years = s.year - date.y;
			let text = '★ ' + getStr(r, 'title');
			if (years > 0 && date.y > 1800) text += ` (${years})`;
			const until = daysUntilNext(date);
			const next = fromDayNumber(todayNumber() + until);
			let soon = '';
			if (next.y === s.year && until <= 30)
				soon = until === 0 ? 'today' : until === 1 ? 'tomorrow' : `in ${until} days`;
			const isToday = now.y === s.year && now.m === month && now.d === date.d;
			return `<div class="line occasion${r.id === s.sel ? ' sel' : ''}${isToday ? ' now' : ''}" data-id="${r.id}">`
				+ `<span class="dnum">${date.d}</span><span class="txt">${esc(text)}</span>`
				+ `<span class="soon">${soon}</span></div>`;
		}).join('');
		return `<div class="monthbox" data-month="${month}"><div class="monthband${rightPage ? ' r' : ''}">${MONTHS[month - 1]}</div>`
			+ `<div class="monthbody">${lines}</div></div>`;
	},

	render() {
		const s = view.anniversary;
		const page = (right) => `<div class="months">${[0, 1, 2].map((i) =>
			this.monthBox(s.firstMonth + i + (right ? 3 : 0), right)).join('')}</div>`;
		const label = `${MONTHS[s.firstMonth - 1]} – ${MONTHS[s.firstMonth + 4]} ${s.year}`;

		pageEl.innerHTML = pageHeader('Anniversary')
			+ `<div class="toolbar">${button('◀', 'prev')}${button('▶', 'next')}${button('Today', 'today')}`
			+ `<strong class="label">${label}</strong><span class="grow"></span>`
			+ `${button('New Occasion', 'new')}${button('Edit', 'edit')}${button('Delete', 'delete')}</div>`
			+ `<div class="body">${spread(page(false), page(true))}</div>`;

		$$('.occasion', pageEl).forEach((el) => {
			el.addEventListener('click', (ev) => { ev.stopPropagation(); s.sel = +el.dataset.id; refresh(); });
			el.addEventListener('dblclick', (ev) => { ev.stopPropagation(); s.sel = +el.dataset.id; this.act('edit'); });
		});
		$$('.monthbox', pageEl).forEach((el) => {
			el.addEventListener('click', () => { if (s.sel !== -1) { s.sel = -1; refresh(); } });
			el.addEventListener('dblclick', () => {
				const month = +el.dataset.month, now = today();
				this.newOn(iso({ y: s.year, m: month, d: now.m === month ? now.d : 1 }));
			});
		});
	},

	newOn(date) {
		openEditor('New occasion', ANNIVERSARY_FIELDS, { date, category: 'Birthday' }, (r) => {
			view.anniversary.sel = store.put('anniversary', r); refresh();
		});
	},

	act(name) {
		const s = view.anniversary;
		switch (name) {
			case 'prev':
				if (s.firstMonth === 7) s.firstMonth = 1; else { s.year--; s.firstMonth = 7; }
				s.sel = -1; refresh(); break;
			case 'next':
				if (s.firstMonth === 1) s.firstMonth = 7; else { s.year++; s.firstMonth = 1; }
				s.sel = -1; refresh(); break;
			case 'today':
				s.year = today().y; s.firstMonth = today().m <= 6 ? 1 : 7; s.sel = -1; refresh(); break;
			case 'new': {
				const now = today();
				this.newOn(iso({ y: s.year, m: s.firstMonth, d: now.m === s.firstMonth ? now.d : 1 }));
				break;
			}
			case 'edit': {
				const record = store.find('anniversary', s.sel);
				if (record) openEditor('Edit occasion', ANNIVERSARY_FIELDS, record, (r) => { store.put('anniversary', r); refresh(); });
				break;
			}
			case 'delete': {
				const record = store.find('anniversary', s.sel);
				if (record && confirm(`Delete "${getStr(record, 'title')}"?`)) {
					store.remove('anniversary', record.id); s.sel = -1; refresh();
				}
				break;
			}
		}
	}
};

const sections = [calendar, makeListSection('todo'), makeListSection('address'),
	makeListSection('calls'), planner, notepad, anniversary];

// ---------------------------------------------------------------- wiring

document.addEventListener('click', (ev) => {
	const tab = ev.target.closest('.tab');
	if (tab) { turnTo(+tab.dataset.section); return; }
	const act = ev.target.closest('[data-act]');
	if (act && pageEl.contains(act)) current.act(act.dataset.act);
});

document.addEventListener('keydown', (ev) => {
	if (ev.metaKey || ev.ctrlKey || ev.altKey || anyDialogOpen()) return;
	if (/^(INPUT|TEXTAREA|SELECT)$/.test(ev.target.tagName)) return;
	const key = ev.key;
	if (/^[1-7]$/.test(key)) { turnTo(+key - 1); return; }
	const map = { n: 'new', e: 'edit', d: 'delete', Delete: 'delete', k: 'toggle',
		ArrowLeft: 'prev', ArrowRight: 'next', t: 'today' };
	if (map[key]) {
		ev.preventDefault();
		current.act(map[key]);
	}
});

// Export / import
$('#menubar').addEventListener('click', (ev) => {
	const item = ev.target.closest('[data-menu]');
	if (!item) return;
	if (item.dataset.menu === 'help') $('#help').showModal();
	else if (item.dataset.menu === 'import') $('#importFile').click();
	else if (item.dataset.menu === 'export') {
		const blob = new Blob([store.exportJson()], { type: 'application/json' });
		const link = document.createElement('a');
		link.href = URL.createObjectURL(blob);
		link.download = `ringbinder-${iso(today())}.json`;
		document.body.appendChild(link);
		link.click();
		link.remove();
		setTimeout(() => URL.revokeObjectURL(link.href), 1000);
	}
});

$('#importFile').addEventListener('change', (ev) => {
	const file = ev.target.files[0];
	ev.target.value = '';
	if (!file) return;
	file.text().then((text) => {
		if (!confirm('Importing replaces everything in this binder with the contents of the file. Continue?')) return;
		try {
			store.importJson(text);
			view.calendar.selId = -1;
			Object.values(view.list).forEach((l) => { l.sel = -1; });
			view.planner.selId = view.notepad.sel = view.anniversary.sel = -1;
			refresh();
		} catch (e) {
			alert('Could not import: ' + e.message);
		}
	});
});

[editor, $('#help')].forEach((d) => d.addEventListener('click', (ev) => {
	if (ev.target === d) d.close();		// click on the backdrop
}));

// Keep "today" honest if the tab stays open past midnight
let lastDay = todayNumber();
setInterval(() => {
	if (todayNumber() !== lastDay && !anyDialogOpen()) {
		lastDay = todayNumber();
		if (view.section !== NOTEPAD) refresh();
	}
}, 60000);

store.load();
view.section = store.data.section;
render();
if (!store.persistent) store.save();	// surfaces the "not saved" notice

})();
