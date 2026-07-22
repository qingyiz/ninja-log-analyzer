#include "gui/AppStyle.h"

QString ninjaAnalyzerStyleSheet()
{
    return QStringLiteral(R"(
        QWidget#centralPanel { background: #F5F6FA; color: #1D2433; }
        QFrame#headerPanel, QFrame#card, QFrame#controlsCard {
            background: #FFFFFF; border: 1px solid #E3E6EE; border-radius: 12px;
        }
        QLabel#brandMark {
            color: #FFFFFF; background: #5B5BD6; border-radius: 11px;
            font-size: 21px; font-weight: 800;
        }
        QLabel#windowTitle { color: #171A2B; font-size: 18px; font-weight: 750; }
        QLabel#subtitle, QLabel#sectionHint, QLabel#scopeHint,
        QLabel#filterStatus, QLabel#timelineStatus { color: #838A9A; font-size: 11px; }
        QLabel#sectionTitle, QLabel#controlsTitle, QLabel#panelTitle {
            color: #252A3A; font-size: 13px; font-weight: 750;
        }
        QLineEdit, QComboBox {
            min-height: 36px; padding: 0 11px; background: #FAFAFC; color: #292E3D;
            border: 1px solid #D9DDE7; border-radius: 8px;
        }
        QLineEdit:focus, QComboBox:focus { border-color: #6565DF; background: #FFFFFF; }
        QPushButton { min-height: 36px; padding: 0 14px; border-radius: 8px; font-weight: 650; }
        QPushButton#primaryButton { color: white; background: #5B5BD6; border: 1px solid #5B5BD6; }
        QPushButton#primaryButton:hover { background: #4848C4; }
        QPushButton#secondaryButton, QPushButton#ghostButton, QPushButton#compactButton {
            color: #555C6D; background: #FFFFFF; border: 1px solid #D9DDE7;
        }
        QPushButton#inlineButton { color: #D7D9E7; background: #2B2E4A; border: 1px solid #414562; }
        QPushButton#inlinePrimaryButton { color: #20223A; background: #B9BBFF; border: 1px solid #B9BBFF; }
        QLabel#diagnostics {
            color: #3E665B; background: #F0F9F5; border: 1px solid #D1EDE1;
            border-radius: 8px; padding: 8px 11px;
        }
        QLabel#initialState { color: #838A9A; font-size: 14px; }
        QTabWidget::pane { border: 0; background: #FFFFFF; }
        QTabBar::tab { color: #747B8C; padding: 13px 19px 11px; font-weight: 650; }
        QTabBar::tab:selected { color: #4E4EC7; border-bottom: 3px solid #5B5BD6; }
        QFrame#conclusionPanel { background: #20223A; border: 1px solid #292C49; border-radius: 12px; }
        QLabel#conclusionEyebrow { color: #A9ACFF; font-size: 10px; font-weight: 750; }
        QLabel#conclusionTitle { color: #FFFFFF; font-size: 17px; font-weight: 750; }
        QLabel#conclusionDetail { color: #BFC3D8; font-size: 11px; }
        QFrame#metricCard, QFrame#innerPanel {
            background: #FAFAFC; border: 1px solid #E5E7EE; border-radius: 10px;
        }
        QFrame#metricCard[tone="primary"] { background: #F0F0FF; border-color: #D7D7FA; }
        QFrame#metricCard[tone="success"] { background: #F4FAF7; border-color: #DDEEE6; }
        QFrame#metricCard QLabel { border: 0; background: transparent; }
        QLabel#metricCaption { color: #6F7687; font-size: 11px; font-weight: 650; }
        QLabel#metricHint, QLabel#panelSubtitle { color: #9A9FAD; font-size: 10px; }
        QLabel#summaryTaskValue, QLabel#summarySpanValue, QLabel#summaryTotalValue,
        QLabel#summaryAverageValue, QLabel#summaryMaximumValue {
            color: #202435; font-size: 21px; font-weight: 780;
        }
        QLabel#summarySpanValue { color: #4E4EC7; font-size: 24px; }
        QTableView, QTableWidget, QListWidget {
            background: #FFFFFF; alternate-background-color: #FAFAFC;
            border: 1px solid #E4E7ED; border-radius: 8px; gridline-color: #ECEEF3;
        }
        QListWidget#insightsList { background: transparent; border: 0; }
        QHeaderView::section {
            color: #6E7585; background: #F5F6F9; border: 0;
            border-bottom: 1px solid #E0E3EA; padding: 8px; font-weight: 650;
        }
        QProgressBar { min-width: 110px; border: 0; border-radius: 5px; background: #ECEEF3; text-align: center; }
        QProgressBar::chunk { background: #7575DE; border-radius: 5px; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical { background: #CDD1DA; border-radius: 4px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
}
