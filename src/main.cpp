#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("KeyExtractor");
    window.resize(480, 240);

    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel("KeyExtractor development environment is ready.", &window);
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
    window.show();

    return app.exec();
}
