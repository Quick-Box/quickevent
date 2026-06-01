------------------------------------;
-- create schema {{eventId}};
------------------------------------;
;
-- create table: enumz;
CREATE TABLE enumz (
	id integer PRIMARY KEY,
	groupName character varying,
	groupId character varying,
	pos integer,
	caption character varying,
	color character varying,
	value character varying,
	CONSTRAINT enumz_unique0 UNIQUE (groupName, groupId)
);
;
-- create table: config;
CREATE TABLE config (
	ckey character varying,
	cname character varying,
	cvalue character varying,
	ctype character varying,
	CONSTRAINT config_pkey PRIMARY KEY (ckey)
);
;
-- create table: stages;
CREATE TABLE stages (
	id integer,
	startDateTime timestamp,
	useAllMaps boolean NOT NULL DEFAULT 0,
	drawingConfig character varying,
	qxApiToken character varying,
	CONSTRAINT stages_pkey PRIMARY KEY (id)
);
;
-- create table: courses;
CREATE TABLE courses (
	id integer PRIMARY KEY,
	name character varying,
	length integer,
	climb integer,
	note character varying,
	mapCount integer
);
CREATE INDEX courses_ix0 ON courses (name);
;
-- create table: codes;
CREATE TABLE codes (
	id integer PRIMARY KEY,
	code integer,
	altCode integer,
	outOfOrder boolean NOT NULL DEFAULT 0,
	radio boolean NOT NULL DEFAULT 0,
	longitude double precision,
	latitude double precision,
	note character varying
);
;
-- create table: coursecodes;
CREATE TABLE coursecodes (
	id integer PRIMARY KEY,
	courseId integer,
	position integer,
	codeId integer,
	CONSTRAINT coursecodes_foreign0 FOREIGN KEY (courseId) REFERENCES courses (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT coursecodes_foreign1 FOREIGN KEY (codeId) REFERENCES codes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX coursecodes_ix2 ON coursecodes (courseId, position);
;
-- create table: classes;
CREATE TABLE classes (
	id integer PRIMARY KEY,
	name character varying,
	CONSTRAINT classes_unique0 UNIQUE (name)
);
;
-- create table: classdefs;
CREATE TABLE classdefs (
	id integer PRIMARY KEY,
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
	resultsPrintTS timestamp,
	lastStartTimeMin integer,
	drawLock boolean NOT NULL DEFAULT 0,
	relayStartNumber integer,
	relayLegCount integer,
	CONSTRAINT classdefs_foreign0 FOREIGN KEY (stageId) REFERENCES stages (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT classdefs_foreign1 FOREIGN KEY (classId) REFERENCES classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT classdefs_foreign2 FOREIGN KEY (courseId) REFERENCES courses (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.vacantsBefore IS 'place n vacants gap before first competitor in class start list';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.vacantEvery IS 'place vacant every n-th competitor in class start list';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.vacantsAfter IS 'place n vacants gap after last competitor in class start list';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.resultsCount IS 'number of finished competitors, when the results were printed';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.resultsPrintTS IS 'when results for this class were printed last time';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN classdefs.drawLock IS 'The draw of this class is prohibited';
;
-- create table: competitors;
CREATE TABLE competitors (
	id integer PRIMARY KEY,
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
	CONSTRAINT competitors_foreign0 FOREIGN KEY (classId) REFERENCES classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX competitors_ix1 ON competitors (importId);
;
-- create table: runs;
CREATE TABLE runs (
	id integer PRIMARY KEY,
	competitorId integer,
	siId integer,
	stageId integer NOT NULL DEFAULT 1,
	leg integer,
	courseId integer,
	relayId integer,
	corridorTime timestamp,
	checkTimeMs integer,
	startTimeMs integer,
	finishTimeMs integer,
	penaltyTimeMs integer,
	timeMs integer,
	isRunning boolean NOT NULL DEFAULT 1,
	disqualified boolean GENERATED ALWAYS AS (disqualifiedByOrganizer OR  misPunch OR  notStart OR  notFinish OR  badCheck OR  overTime OR notCompeting) STORED,
	disqualifiedByOrganizer boolean NOT NULL DEFAULT 0,
	notCompeting boolean NOT NULL DEFAULT 0,
	misPunch boolean NOT NULL DEFAULT 0,
	notStart boolean NOT NULL DEFAULT 0,
	notFinish boolean NOT NULL DEFAULT 0,
	badCheck boolean NOT NULL DEFAULT 0,
	overTime boolean NOT NULL DEFAULT 0,
	cardLent boolean NOT NULL DEFAULT 0,
	cardReturned boolean NOT NULL DEFAULT 0,
	importId integer,
	CONSTRAINT runs_foreign0 FOREIGN KEY (competitorId) REFERENCES competitors (id) ON UPDATE RESTRICT ON DELETE RESTRICT,
	CONSTRAINT runs_foreign1 FOREIGN KEY (stageId) REFERENCES stages (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX runs_ix2 ON runs (relayId, leg);
CREATE INDEX runs_ix3 ON runs (stageId, siId);
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.corridorTime IS 'DateTime when competitor entered start corridor. (Experimental)';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.checkTimeMs IS 'in miliseconds';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.startTimeMs IS 'in miliseconds';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.finishTimeMs IS 'in miliseconds';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.penaltyTimeMs IS 'in miliseconds';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.timeMs IS 'in miliseconds';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.isRunning IS 'Competitor is running in this stage';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.disqualifiedByOrganizer IS 'Competitor is disqualified by organizer for breaking rules, etc.';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN runs.notCompeting IS 'Competitor does run in this stage but not competing';
;
-- create table: relays;
CREATE TABLE relays (
	id integer PRIMARY KEY,
	number integer,
	classId integer,
	club character varying,
	name character varying,
	note character varying,
	importId integer,
	isRunning boolean NOT NULL DEFAULT 1,
	CONSTRAINT relays_foreign0 FOREIGN KEY (classId) REFERENCES classes (id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX relays_ix1 ON relays (club, name);
CREATE INDEX relays_ix2 ON relays (number);
;
-- create table: runlaps;
CREATE TABLE runlaps (
	id integer PRIMARY KEY,
	runId integer,
	position integer,
	code integer,
	stpTimeMs integer,
	lapTimeMs integer
);
CREATE INDEX runlaps_ix0 ON runlaps (runId, position);
CREATE INDEX runlaps_ix1 ON runlaps (position, stpTimeMs);
CREATE INDEX runlaps_ix2 ON runlaps (position, lapTimeMs);
;
-- create table: clubs;
CREATE TABLE clubs (
	id integer PRIMARY KEY,
	name character varying,
	abbr character varying,
	importId integer
);
CREATE INDEX clubs_ix0 ON clubs (abbr);
;
-- create table: registrations;
CREATE TABLE registrations (
	id integer PRIMARY KEY,
	firstName character varying,
	lastName character varying,
	registration character varying(10),
	licence character varying(1),
	clubAbbr character varying,
	country character varying,
	siId integer,
	importId integer
);
CREATE INDEX registrations_ix0 ON registrations (registration);
;
-- create table: cards;
CREATE TABLE cards (
	id integer PRIMARY KEY,
	runId integer,
	runIdAssignTS timestamp,
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
CREATE INDEX cards_ix0 ON cards (readerConnectionId);
CREATE INDEX cards_ix1 ON cards (printerConnectionId);
CREATE INDEX cards_ix2 ON cards (stageId, siId);
CREATE INDEX cards_ix3 ON cards (runId);
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.stageId IS 'We cannot take stageId from runId linked table, because we need select cards for stage even without runId assigned';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.checkTime IS 'seconds in range 0 - 12 hours';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.startTime IS 'seconds in range 0 - 12 hours';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.finishTime IS 'seconds in range 0 - 12 hours';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.punches IS 'JSON of format [[code, time, msec, day_of_week, week_cnt], ...]}';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.data IS 'JSON of auxiliary card data like Siac battery status, etc.';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.readerConnectionId IS 'connection id of QuickEvent instance which has read this card';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN cards.printerConnectionId IS 'connection id of QuickEvent instance which has printed this strip';
;
-- create table: punches;
CREATE TABLE punches (
	id integer PRIMARY KEY,
	code integer,
	siId integer,
	time integer,
	msec integer,
	stageId integer,
	runId integer,
	timeMs integer,
	runTimeMs integer
);
CREATE INDEX punches_ix0 ON punches (stageId, code);
CREATE INDEX punches_ix1 ON punches (runId);
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN punches.time IS 'seconds in range 0 - 12 hours';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN punches.msec IS 'msec part od punch time';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN punches.stageId IS 'We cannot take stageId from runId linked table, because we need select punches for stage even without runId assigned';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN punches.timeMs IS 'in miliseconds since event start';
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN punches.runTimeMs IS 'in miliseconds since runner event start';
;
-- create table: stationsbackup;
CREATE TABLE stationsbackup (
	id integer PRIMARY KEY,
	stageId integer,
	stationNumber integer,
	siId integer,
	punchDateTime timestamp,
	cardErr boolean,
	CONSTRAINT stationsbackup_unique0 UNIQUE (stageId, stationNumber, siId, punchDateTime)
);
;
-- create table: lentcards;
CREATE TABLE lentcards (
	siId integer PRIMARY KEY,
	ignored boolean NOT NULL DEFAULT 0,
	note character varying
);
;
-- create table: qxchanges;
CREATE TABLE qxchanges (
	id integer PRIMARY KEY,
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
	created timestamp,
	lock_number integer,
	CONSTRAINT qxchanges_unique0 UNIQUE (stage_id, change_id)
);
-- comments not suported for driver: SQLITE
-- COMMENT ON COLUMN qxchanges.orig_data IS 'Store data overriden by change here to enable change rollback.';
;
------------------------------------;
-- insert initial data;
------------------------------------;
;
-- insert into table: config;
INSERT INTO config (ckey, cname, cvalue, ctype) VALUES 
('db.version', 'Data version', '30500', 'int');
