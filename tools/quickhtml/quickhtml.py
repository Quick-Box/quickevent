#!/usr/bin/python
# vim:fileencoding=utf-8:tabstop=4:shiftwidth=4:expandtab

############################################
#  Generate HTML results from Jinja2 template
#
#  Author: Martin Horak
#  Version: 1.1
#  Date: 14. 7. 2021
#
############################################
import argparse
import pathlib
import logging
import sys, os, time
from datetime import datetime
from jinja2 import Environment, FileSystemLoader, select_autoescape

## Constants ## ==========================
############### ==========================
# Translate UTF-8 national characters to ASCII for file names.
tr1 = 'áčďéěíňóřšťůúýžľĺÁČĎÉĚÍŇÓŘŠŤŮÚÝŽĽĹ'
tr2 = 'acdeeinorstuuyzllacdeeinorstuuyzll'
trans = str.maketrans(tr1, tr2)


## Functions ## ============================
############### ============================
def timefmt(milisecs, print_hours=False):
    '''Format time in MS to [H]:MM:SS'''
    if milisecs == None:
        return '--'
    secs = int(milisecs / 1000)
    if print_hours:
        hours = secs // 3600
    else:
        hours = 0
    mins = (secs - hours * 3600) // 60
    secs = secs % 60
    if hours > 0:
        out = f'{hours}:'
    else:
        out = ''
    out += f'{mins:02}:'
    out += f'{secs:02}'
    return out

## Main ## =================================
########## =================================
def main():

    modes = ['r', 's', 't', 'a']

    parser = argparse.ArgumentParser()

    parser.add_argument("event", help="Event - name of DB schema (psql) or filename (sqlite)")

    parser.add_argument("--sql-driver", help="SQL database to connect (psql|sqlite) [psql]", default="psql", choices=['psql', 'sqlite'])
    parser.add_argument("-s", "--sql-server", help="Server to connect [localhost]", default="localhost")
    parser.add_argument("--sql-port", help="TCP port to connect [5432]", type=int, default=5432)

    parser.add_argument("-u", "--user", help="User for DB connection [quickevent]", default="quickevent")
    parser.add_argument("-p", "--password", help="Password for DB connection [None]")

    parser.add_argument("-b", "--sql-database", help="Database name [quickevent]", default="quickevent")
    parser.add_argument("-n", "--stage", help="Stage number [1]", type=int, default=0)

    parser.add_argument("-m", "--mode", help="Output mode {results|starts|total|all} [r,s]", choices=modes, action='append')
    parser.add_argument("--main-index", help="Create main index file [Automatically on in 'all' mode]", action='store_true')

    parser.add_argument("-d", "--html-dir", help="Folder where HTML pages will be stored [./html]", type=pathlib.Path, default="./html")
    parser.add_argument("-r", "--refresh-interval", help="Refresh time interval in seconds [60]; 0 = once and exit", type=int, default=60)

    parser.add_argument("-t", "--templates", help="Templates folder", default='templates')
    parser.add_argument("-l", "--log-file", help="Log activity into file", default=None)

    parser.add_argument("-v", "--verbose", help="More information", action="count", default=0)
    parser.add_argument("-q", "--quiet", help="Less information", action="count", default=0)

    args = parser.parse_args()
    main_index = args.main_index
    if args.mode == None or 'a' in args.mode:
        mode = ['s', 'r']
        main_index = True
        if 'a' in args.mode:
            mode.append('t')
    else:
        mode = args.mode

# Set proper loglevel
    args.verbose -= args.quiet
    loglevel = 30 - 10 * args.verbose
    if loglevel < 10:
        loglevel = 10
    elif loglevel > 50:
        loglevel = 50

    logging.basicConfig(level=loglevel, filename=args.log_file, format='{asctime}: {levelname}: {message}', style='{', datefmt='%d.%m.%Y,%H:%M:%S')

    if args.log_file != None:
        print(f"Logging to {args.log_file}.", file=sys.stderr)

# Connect to database
    try:
        if args.sql_driver == "psql":
            from psycopg2 import connect, OperationalError
            dbcon = connect(host=args.sql_server, database=args.sql_database, user=args.user, password=args.password)
            is_bigdb = True
            plc = "%s"
        else:
            from sqlite3 import connect, OperationalError
            if not os.path.isfile(args.event):
                raise OperationalError("DB does not exist.")
            dbcon = connect(database=args.event)
            is_bigdb = False
            plc = "?"

    except OperationalError:
        logging.critical("Cannot connect to database.")
        return


# Initialize DB cursor
    cur = dbcon.cursor()
    if is_bigdb:
        cur.execute(f"SELECT count(*) FROM pg_catalog.pg_namespace WHERE nspname={plc}",
                    (args.event,))
        if cur.fetchone()[0] != 1:
            logging.critical(f"Event {args.event} cannot be found in database.")
            return
        cur.execute("SET SCHEMA %s", (args.event,))
        logging.info(f'Using "big" database, schema {args.event}')

# Initialize Jinja templates
    env = Environment(loader=FileSystemLoader(args.templates), autoescape=select_autoescape())
    env.filters['timefmt'] = timefmt

# Create dir for total results
    if 't' in mode:
        outdir_total = args.html_dir.joinpath(f'total')
        try:
            outdir_total.mkdir(parents=True, exist_ok=True)
        except OSError:
            logging.critical(f"Cannot create output directories ({outdir})")
            sys.exit(1)


# Main loop generating HTML reports. When stage is not specified, it can dynamically change
# according to current stage in database (manipulated via QuickEvent program)
    while True:
# Read event data
        cur.execute("SELECT id, cvalue FROM config WHERE id LIKE 'event.%'")
        event = {}
        for i in cur:
            (_, field) = i[0].split('.', 2)
            event[field] = i[1]

        if args.stage > 0:
            stage = min(args.stage, int(event['stageCount']))
        else:
            stage = int(event.get('currentStageId', event['stageCount']))
        outdir_stage = args.html_dir.joinpath(f'E{stage}')
        try:
            outdir_stage.joinpath('results').mkdir(parents=True, exist_ok=True)
            outdir_stage.joinpath('starts').mkdir(parents=True, exist_ok=True)
        except OSError:
            logging.critical(f"Cannot create output directories ({outdir_stage})")
            sys.exit(1)

# Read classes list
        cur.execute(f"SELECT classes.id, name FROM classes INNER JOIN classdefs ON classdefs.classId=classes.id AND (classdefs.stageId={plc}) ORDER BY name", (stage,))
        classes = []
        for i in cur:
            classes.append({'id': i[0], 'name': i[1], 'ascii': i[1].translate(trans).lower()})

# Generate main index file
        if main_index:
            tmpl_index = env.get_template("main_index.html")
            tmpl_index.stream({'link_totals': 't' in mode, 'classes': classes, 'event': event, 'stage': stage}).dump(f'{args.html_dir}/index.html')
            logging.info(f'Written main index -> {args.html_dir}/index.html')
            tmpl_index = env.get_template("index.html")
            tmpl_index.stream({'classes': classes, 'event': event, 'stage': stage}).dump(f'{outdir_stage}/index.html')
            logging.info(f'Written stage index -> {outdir_stage}/index.html')

# Generate separate index files
        if 'r' in mode:
            tmpl_index = env.get_template("results/index.html")
            tmpl_index.stream({'classes': classes, 'event': event, 'stage': stage}).dump(f'{outdir_stage}/results/index.html')
            logging.info(f'Written results index -> {outdir_stage}/results/index.html')
        if 's' in mode:
            tmpl_index = env.get_template("startlists/index.html")
            tmpl_index.stream({'classes': classes, 'event': event, 'stage': stage}).dump(f'{outdir_stage}/starts/index.html')
            logging.info(f'Written results index -> {outdir_stage}/starts/index.html')
        if 't' in mode:
            tmpl_index = env.get_template("total/index.html")
            tmpl_index.stream({'classes': classes, 'event': event, 'stage': stage}).dump(f'{outdir_total}/index.html')
            logging.info(f'Written totals index -> {outdir_total}/index.html')

# Classes loop
        for cls in classes:
            cur.execute(f"SELECT classes.name, courses.length, courses.climb FROM classes LEFT JOIN classdefs ON classdefs.classId=classes.id AND (classdefs.stageId={plc}) INNER JOIN courses ON courses.id=classdefs.courseId WHERE (classes.id={plc})", (stage, cls['id']))
            r = cur.fetchone()
            cls['length'] = r[1]
            cls['climb'] = r[2]

# Count total results
            if 't' in mode:
# Read competitors
# totals: { id: [reg, name, total_time, race_stat, notcompeting, [E1_time, E1_status, E1_notcomp, E1_rank], [E2_time, ...]] }
                cur.execute(f"""
SELECT
  id, registration,
  COALESCE(lastName, '') || ' ' || COALESCE(firstName, '') AS fullName
FROM competitors
WHERE
  classid = {plc}
ORDER BY id
                            """, (cls['id'], ))
                totals = {}
                for i in cur:
                    totals[i[0]] = [i[1], i[2], 0, 0, False]


                for stg in range(1, stage+1):
                    cur.execute(f"""
SELECT
  comp.id,
  e.timems,
  e.isrunning, e.disqualified, e.notcompeting
FROM
  competitors AS comp,
  runs as e
WHERE
  comp.classid = {plc} AND
  comp.id = e.competitorid AND
  e.stageid = {plc}
ORDER BY
  e.notcompeting, not(e.isrunning), e.disqualified,
  e.timems
                                """, (cls['id'], stg))
                    rank = 0
                    for i in cur:
                        rank += 1
                        cid = i[0]
                        status = 0
                        if not i[2]:
                            status = 2 # DNS
                        elif i[3]:
                            status = 1 # DISQ
                        elif i[1] == None:
                            status = 3 # DNF

                        stg_res = [i[1], status, rank]

                        # Add time to total
                        if status == 0:
                            totals[cid][2] += i[1]
                        else:
                            totals[cid][3] += status
                        totals[cid][4] |= i[4]
                        totals[cid].append(stg_res)

                results = []
                for i in sorted(totals.values(), key=lambda x: (x[4], x[3], x[2])):
                    stages = []
                    for j in range(1, stage+1):
                        stages.append({
                            'time': i[j+4][0],
                            'rank': i[j+4][2],
                            'stat': i[j+4][1]
                        })

                    results.append({
                            'registration': i[0],
                            'fullname': i[1],
                            'totaltime': i[2],
                            'stat': i[3],
                            'notcompeting': i[4],
                            'stages': stages
                    })

                filename = cls['ascii']
                tmpl_class = env.get_template("total/class.html")
                tmpl_class.stream({'classes': classes, 'cls': cls, 'event': event, 'stage': stage, 'results': results, 'curtime': datetime.now()}).dump(f'{outdir_total}/{filename}.html')
                logging.info(f'Written total results -> {outdir_total}/{filename}.html')

# Read results
            if 'r' in mode:
                cur.execute(f"SELECT competitors.registration, competitors.lastName, competitors.firstName, COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '') AS fullName, runs.siid, runs.leg, runs.relayid, runs.checktimems, runs.starttimems, runs.finishtimems, runs.penaltytimems, runs.timems, runs.notcompeting, runs.disqualified, runs.mispunch, runs.badcheck FROM competitors JOIN runs ON runs.competitorId=competitors.id AND (runs.stageId={plc} AND runs.isRunning AND runs.finishTimeMs>0) WHERE (competitors.classId={plc}) ORDER BY runs.notCompeting, runs.disqualified, runs.timeMs", (stage, cls['id']))

                competitors = []
                for i in cur:

                    competitors.append({
                        'registration': i[0],
                        'lastname': i[1],
                        'firstname': i[2],
                        'fullname': i[3],
                        'siid': i[4],
                        'leg': i[5],
                        'relayid': i[6],
                        'checktime': i[7],
                        'starttime': i[8],
                        'finishtime': i[9],
                        'penaltytime': i[10],
                        'time': i[11],
                        'notcompeting': i[12],
                        'disq': i[13],
                        'mispunch': i[14],
                        'badcheck': i[15]
                    })

                filename = cls['ascii']
                tmpl_class = env.get_template("results/class.html")
                tmpl_class.stream({'classes': classes, 'cls': cls, 'event': event, 'stage': stage, 'competitors': competitors, 'curtime': datetime.now()}).dump(f'{outdir_stage}/results/{filename}.html')
                logging.info(f'Written results -> {outdir_stage}/results/{filename}.html')

# Read startlists
            if 's' in mode:
                cur.execute(f"SELECT startdatetime FROM stages WHERE (id={plc})", (stage, ))

                start_dt = cur.fetchone()[0]

                cur.execute(f"SELECT competitors.registration, competitors.lastName, competitors.firstName, COALESCE(competitors.lastName, '') || ' ' || COALESCE(competitors.firstName, '') AS fullName, runs.siid, runs.leg, runs.relayid, runs.starttimems, runs.notcompeting FROM competitors JOIN runs ON runs.competitorId=competitors.id AND runs.stageId={plc} WHERE (competitors.classId={plc}) ORDER BY runs.starttimems, fullName", (stage, cls['id']))

                filename = cls['ascii']

                competitors = []
                for i in cur:

                    competitors.append({
                        'registration': i[0],
                        'lastname': i[1],
                        'firstname': i[2],
                        'fullname': i[3],
                        'siid': i[4],
                        'leg': i[5],
                        'relayid': i[6],
                        'starttime': i[7],
                        'notcompeting': i[8],
                    })
                tmpl_class = env.get_template("startlists/class.html")
                tmpl_class.stream({'classes': classes, 'cls': cls, 'event': event, 'stage': stage, 'competitors': competitors, 'start_dt': start_dt, 'curtime': datetime.now()}).dump(f'{outdir_stage}/starts/{filename}.html')
                logging.info(f'Written startlists -> {outdir_stage}/starts/{filename}.html')

        logging.info("Writing HTML files completed.")
        if args.refresh_interval == 0:
            break
        time.sleep(args.refresh_interval)
## Main end =================================

## Main run =================================
########### =================================
if __name__ == '__main__':
    main()

## End of program # =========================
################### =========================
