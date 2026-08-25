import qf.qmlreports 1.0
import shared.qml.reports 1.0
import "qrc:/quickevent/core/js/ogtime.js" as OGTime

Frame {
	id: root
	property var eventConfig
	property var stageConfig
	property int stageId
	property string reportTitle
	property bool showStageNumber: true
	Para {
		textStyle: TextStyle {basedOn: "big"}
		textFn: function() {
			var ret = root.reportTitle;
			if(root.showStageNumber) {
				var stage_cnt = root.eventConfig ? root.eventConfig.stageCount : 0
				if(stage_cnt > 1 && stageId > 0)
					ret = "E" + root.stageId + " " + ret;
			}
			return ret;
		}
	}
	Para {
		textStyle: myStyle.textStyleBold
		textFn: function() { var event_cfg = root.eventConfig; return event_cfg ? event_cfg.name : ""; }
	}
	Para {
		textFn: function() {
			var start = root.stageConfig ? root.stageConfig.startDateTime : null;
			if(!start) {
				var event_cfg = root.eventConfig;
				if(event_cfg) {
					//console.info(JSON.stringify(event_cfg))
					start = event_cfg.dateTime;
					if(!start)
						start = event_cfg.date;
				}
			}
			return start? start: "";
		}
	}
	Para {
		textFn: function() { var event_cfg = root.eventConfig; return event_cfg ? event_cfg.place : ""; }
	}
}
