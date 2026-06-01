------------------------------------;
-- create schema {{eventId}};
------------------------------------;
CREATE SCHEMA {{eventId}};
;
-- create table: {{eventId}}.enumz;
CREATE TABLE {{eventId}}.enumz (
	id serial PRIMARY KEY,
	groupName character varying,
	groupId character varying,
	pos integer,
	caption character varying,
	color character varying,
	value character varying,
	CONSTRAINT enumz_unique0 UNIQUE (groupName, groupId)
);
;
-- create table: {{eventId}}.config;
CREATE TABLE {{eventId}}.config (
	ckey character varying,
	cname character varying,
	cvalue character varying,
	ctype character varying,
	CONSTRAINT config_pkey PRIMARY KEY (ckey)
);
;
-- create table: {{eventId}}.stages;
CREATE TABLE {{eventId}}.stages (
	id integer,
	startDateTime timestamp with time zone,
	useAllMaps boolean NOT NULL DEFAULT false,
	drawingConfig character varying,
	qxApiToken character varying,
	CONSTRAINT stages_pkey PRIMARY KEY (id)
);
;
-- create table: {{eventId}}.courses;
CREATE TABLE {{eventId}}.courses (
	id serial PRIMARY KEY,
	name character varying,
	length integer,
	climb integer,
	note character varying,
	mapCount integer
);
CREATE INDEX courses_ix0 ON {{eventId}}.courses (name);
;
-- create table: {{eventId}}.codes;
CREATE TABLE {{eventId}}.codes (
	id serial PRIMARY KEY,
	code integer,
	altCode integer,
	outOfOrder boolean NOT NULL DEFAULT false,
	radio boolean NOT NULL DEFAULT false,
	longitude double precision,
	latitude double precision,
	note character varying
);
;
-- create table: {{eventId}}.coursecodes;
CREATE TABLE {{eventId}}.coursecodes (
	id serial PRIMARY KEY,
	courseId integer,
	position integer,
	codeId integer,
	CONSTRAINT coursecodes_foreign0 FOREIGN KEY (courseId) REFERENCES {{eventId}}.courses (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT coursecodes_foreign1 FOREIGN KEY (codeId) REFERENCES {{eventId}}.codes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX coursecodes_ix2 ON {{eventId}}.coursecodes (courseId, position);
;
-- create table: {{eventId}}.classes;
CREATE TABLE {{eventId}}.classes (
	id serial PRIMARY KEY,
	name character varying,
	CONSTRAINT classes_unique0 UNIQUE (name)
);
;
-- create table: {{eventId}}.classdefs;
CREATE TABLE {{eventId}}.classdefs (
	id serial PRIMARY KEY,
	classId integer,
	stageId integer,
	courseId integer,
	startSlotIndex integer NOT NULL DEFAULT -1,
	startTimeMin integer,
	startIntervalMin integer,
	vacantsBefore integer,
	vacantEvery integer,
	vacantsAfter integer,
	mapCount integer,
	resultsCount integer,
	resultsPrintTS timestamp with time zone,
	lastStartTimeMin integer,
	drawLock boolean NOT NULL DEFAULT false,
	relayStartNumber integer,
	relayLegCount integer,
	CONSTRAINT classdefs_foreign0 FOREIGN KEY (stageId) REFERENCES {{eventId}}.stages (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT classdefs_foreign1 FOREIGN KEY (classId) REFERENCES {{eventId}}.classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT classdefs_foreign2 FOREIGN KEY (courseId) REFERENCES {{eventId}}.courses (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
COMMENT ON COLUMN {{eventId}}.classdefs.vacantsBefore IS 'place n vacants gap before first competitor in class start list';
COMMENT ON COLUMN {{eventId}}.classdefs.vacantEvery IS 'place vacant every n-th competitor in class start list';
COMMENT ON COLUMN {{eventId}}.classdefs.vacantsAfter IS 'place n vacants gap after last competitor in class start list';
COMMENT ON COLUMN {{eventId}}.classdefs.resultsCount IS 'number of finished competitors, when the results were printed';
COMMENT ON COLUMN {{eventId}}.classdefs.resultsPrintTS IS 'when results for this class were printed last time';
COMMENT ON COLUMN {{eventId}}.classdefs.drawLock IS 'The draw of this class is prohibited';
;
-- create table: {{eventId}}.competitors;
CREATE TABLE {{eventId}}.competitors (
	id serial PRIMARY KEY,
	startNumber integer,
	classId integer,
	firstName character varying,
	lastName character varying,
	registration character varying(10),
	iofId integer,
	licence character varying(1),
	club character varying,
	country character varying,
	siId integer,
	ranking integer,
	note character varying,
	importId integer,
	CONSTRAINT competitors_foreign0 FOREIGN KEY (classId) REFERENCES {{eventId}}.classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX competitors_ix1 ON {{eventId}}.competitors (importId);
;
-- create table: {{eventId}}.runs;
CREATE TABLE {{eventId}}.runs (
	id serial PRIMARY KEY,
	competitorId integer,
	siId integer,
	stageId integer NOT NULL DEFAULT 1,
	leg integer,
	courseId integer,
	relayId integer,
	corridorTime timestamp with time zone,
	checkTimeMs integer,
	startTimeMs integer,
	finishTimeMs integer,
	penaltyTimeMs integer,
	timeMs integer,
	isRunning boolean NOT NULL DEFAULT true,
	disqualified boolean GENERATED ALWAYS AS (disqualifiedByOrganizer OR  misPunch OR  notStart OR  notFinish OR  badCheck OR  overTime OR notCompeting) STORED,
	disqualifiedByOrganizer boolean NOT NULL DEFAULT false,
	notCompeting boolean NOT NULL DEFAULT false,
	misPunch boolean NOT NULL DEFAULT false,
	notStart boolean NOT NULL DEFAULT false,
	notFinish boolean NOT NULL DEFAULT false,
	badCheck boolean NOT NULL DEFAULT false,
	overTime boolean NOT NULL DEFAULT false,
	cardLent boolean NOT NULL DEFAULT false,
	cardReturned boolean NOT NULL DEFAULT false,
	importId integer,
	CONSTRAINT runs_foreign0 FOREIGN KEY (competitorId) REFERENCES {{eventId}}.competitors (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT runs_foreign1 FOREIGN KEY (stageId) REFERENCES {{eventId}}.stages (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX runs_ix2 ON {{eventId}}.runs (relayId, leg);
CREATE INDEX runs_ix3 ON {{eventId}}.runs (stageId, siId);
COMMENT ON COLUMN {{eventId}}.runs.corridorTime IS 'DateTime when competitor entered start corridor. (Experimental)';
COMMENT ON COLUMN {{eventId}}.runs.checkTimeMs IS 'in miliseconds';
COMMENT ON COLUMN {{eventId}}.runs.startTimeMs IS 'in miliseconds';
COMMENT ON COLUMN {{eventId}}.runs.finishTimeMs IS 'in miliseconds';
COMMENT ON COLUMN {{eventId}}.runs.penaltyTimeMs IS 'in miliseconds';
COMMENT ON COLUMN {{eventId}}.runs.timeMs IS 'in miliseconds';
COMMENT ON COLUMN {{eventId}}.runs.isRunning IS 'Competitor is running in this stage';
COMMENT ON COLUMN {{eventId}}.runs.disqualifiedByOrganizer IS 'Competitor is disqualified by organizer for breaking rules, etc.';
COMMENT ON COLUMN {{eventId}}.runs.notCompeting IS 'Competitor does run in this stage but not competing';
;
-- create table: {{eventId}}.relays;
CREATE TABLE {{eventId}}.relays (
	id serial PRIMARY KEY,
	number integer,
	classId integer,
	club character varying,
	name character varying,
	note character varying,
	importId integer,
	isRunning boolean NOT NULL DEFAULT true,
	CONSTRAINT relays_foreign0 FOREIGN KEY (classId) REFERENCES {{eventId}}.classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX relays_ix1 ON {{eventId}}.relays (club, name);
CREATE INDEX relays_ix2 ON {{eventId}}.relays (number);
;
-- create table: {{eventId}}.runlaps;
CREATE TABLE {{eventId}}.runlaps (
	id serial PRIMARY KEY,
	runId integer,
	position integer,
	code integer,
	stpTimeMs integer,
	lapTimeMs integer
);
CREATE INDEX runlaps_ix0 ON {{eventId}}.runlaps (runId, position);
CREATE INDEX runlaps_ix1 ON {{eventId}}.runlaps (position, stpTimeMs);
CREATE INDEX runlaps_ix2 ON {{eventId}}.runlaps (position, lapTimeMs);
;
-- create table: {{eventId}}.clubs;
CREATE TABLE {{eventId}}.clubs (
	id serial PRIMARY KEY,
	name character varying,
	abbr character varying,
	importId integer
);
CREATE INDEX clubs_ix0 ON {{eventId}}.clubs (abbr);
;
-- create table: {{eventId}}.registrations;
CREATE TABLE {{eventId}}.registrations (
	id serial PRIMARY KEY,
	firstName character varying,
	lastName character varying,
	registration character varying(10),
	licence character varying(1),
	clubAbbr character varying,
	country character varying,
	siId integer,
	importId integer
);
CREATE INDEX registrations_ix0 ON {{eventId}}.registrations (registration);
;
-- create table: {{eventId}}.cards;
CREATE TABLE {{eventId}}.cards (
	id serial PRIMARY KEY,
	runId integer,
	runIdAssignTS timestamp with time zone,
	runIdAssignError character varying,
	stageId integer,
	stationNumber integer DEFAULT 0,
	siId integer,
	checkTime integer,
	startTime integer,
	finishTime integer,
	punches character varying(65536),
	data character varying,
	readerConnectionId integer,
	printerConnectionId integer
);
CREATE INDEX cards_ix0 ON {{eventId}}.cards (readerConnectionId);
CREATE INDEX cards_ix1 ON {{eventId}}.cards (printerConnectionId);
CREATE INDEX cards_ix2 ON {{eventId}}.cards (stageId, siId);
CREATE INDEX cards_ix3 ON {{eventId}}.cards (runId);
COMMENT ON COLUMN {{eventId}}.cards.stageId IS 'We cannot take stageId from runId linked table, because we need select cards for stage even without runId assigned';
COMMENT ON COLUMN {{eventId}}.cards.checkTime IS 'seconds in range 0 - 12 hours';
COMMENT ON COLUMN {{eventId}}.cards.startTime IS 'seconds in range 0 - 12 hours';
COMMENT ON COLUMN {{eventId}}.cards.finishTime IS 'seconds in range 0 - 12 hours';
COMMENT ON COLUMN {{eventId}}.cards.punches IS 'JSON of format [[code, time, msec, day_of_week, week_cnt], ...]}';
COMMENT ON COLUMN {{eventId}}.cards.data IS 'JSON of auxiliary card data like Siac battery status, etc.';
COMMENT ON COLUMN {{eventId}}.cards.readerConnectionId IS 'connection id of QuickEvent instance which has read this card';
COMMENT ON COLUMN {{eventId}}.cards.printerConnectionId IS 'connection id of QuickEvent instance which has printed this strip';
;
-- create table: {{eventId}}.punches;
CREATE TABLE {{eventId}}.punches (
	id serial PRIMARY KEY,
	code integer,
	siId integer,
	time integer,
	msec integer,
	stageId integer,
	runId integer,
	timeMs integer,
	runTimeMs integer
);
CREATE INDEX punches_ix0 ON {{eventId}}.punches (stageId, code);
CREATE INDEX punches_ix1 ON {{eventId}}.punches (runId);
COMMENT ON COLUMN {{eventId}}.punches.time IS 'seconds in range 0 - 12 hours';
COMMENT ON COLUMN {{eventId}}.punches.msec IS 'msec part od punch time';
COMMENT ON COLUMN {{eventId}}.punches.stageId IS 'We cannot take stageId from runId linked table, because we need select punches for stage even without runId assigned';
COMMENT ON COLUMN {{eventId}}.punches.timeMs IS 'in miliseconds since event start';
COMMENT ON COLUMN {{eventId}}.punches.runTimeMs IS 'in miliseconds since runner event start';
;
-- create table: {{eventId}}.stationsbackup;
CREATE TABLE {{eventId}}.stationsbackup (
	id serial PRIMARY KEY,
	stageId integer,
	stationNumber integer,
	siId integer,
	punchDateTime timestamp with time zone,
	cardErr boolean,
	CONSTRAINT stationsbackup_unique0 UNIQUE (stageId, stationNumber, siId, punchDateTime)
);
;
-- create table: {{eventId}}.lentcards;
CREATE TABLE {{eventId}}.lentcards (
	siId integer PRIMARY KEY,
	ignored boolean NOT NULL DEFAULT false,
	note character varying
);
;
-- create table: {{eventId}}.qxchanges;
CREATE TABLE {{eventId}}.qxchanges (
	id serial PRIMARY KEY,
	stage_id integer,
	change_id integer,
	data_id integer,
	data_type character varying,
	data character varying,
	orig_data character varying,
	source character varying,
	user_id character varying,
	status character varying,
	status_message character varying,
	created timestamp with time zone,
	lock_number integer,
	CONSTRAINT qxchanges_unique0 UNIQUE (stage_id, change_id)
);
COMMENT ON COLUMN {{eventId}}.qxchanges.orig_data IS 'Store data overriden by change here to enable change rollback.';
;
------------------------------------;
-- insert initial data;
------------------------------------;
;
-- insert into table: {{eventId}}.config;
INSERT INTO {{eventId}}.config (ckey, cname, cvalue, ctype) VALUES 
('db.version', 'Data version', '30500', 'int');
