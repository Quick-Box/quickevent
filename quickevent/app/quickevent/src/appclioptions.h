#ifndef APPCLIOPTIONS_H
#define APPCLIOPTIONS_H

#include <qf/core/utils/clioptions.h>

#include <QUrl>

class AppCliOptions : public qf::core::utils::ConfigCLIOptions
{
	Q_OBJECT
private:
	typedef qf::core::utils::ConfigCLIOptions Super;
public:
	AppCliOptions(QObject *parent = nullptr);
	~AppCliOptions() Q_DECL_OVERRIDE {}

	CLIOPTION_GETTER_SETTER(QString, l, setL, ocale)
	CLIOPTION_GETTER_SETTER2(double, "app.fontScale", a, setA, ppFontScale)
};

#endif // APPCLIOPTIONS_H
