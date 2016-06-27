import QtQml 2.0
import qf.core 1.0
import qf.qmlwidgets 1.0

Plugin {
	id: root

	property QfObject internals: QfObject
	{
		ThisPartWidget{
			id: thisPart
		}
	}

	/*
	property list<Action> actions: [
		Action {
			id: actLAboutQt
			text: qsTr('About &Qt')
			onTriggered: {
				MessageBoxSingleton.aboutQt();
			}
		}
	]
		*/

	onInstalled:
	{
		//FrameWork.menuBar.actionForPath('help').addActionInto(actLAboutQt);
		//FrameWork.menuBar.actionForPath('help').addSeparatorInto();
		FrameWork.addPartWidget(thisPart, manifest.featureId);
	}
}
