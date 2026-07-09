import sys

from PyQt5.QtWidgets import QApplication

from controller import AppController


def main() -> int:
    app = QApplication(sys.argv)
    controller = AppController()
    controller.show()
    app.aboutToQuit.connect(controller.shutdown)
    return app.exec_()


if __name__ == "__main__":
    raise SystemExit(main())
