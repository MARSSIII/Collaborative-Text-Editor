#include "client/editor_widget.h"

#include "collab/operation.h"

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTest>

Q_DECLARE_METATYPE(std::vector<collab::Operation>)

using namespace collab_client;

class EditorWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        qRegisterMetaType<std::vector<collab::Operation>>(
            "std::vector<collab::Operation>");
    }

    void typesSingleAsciiChar() {
        EditorWidget w;
        w.setIdentity(1, 0);
        w.resetContent("");

        QSignalSpy spy(&w, &EditorWidget::localOperationsGenerated);
        QTest::keyClick(&w, 'X');

        QCOMPARE(spy.count(), 1);
        auto ops = spy.first().at(0).value<std::vector<collab::Operation>>();
        QCOMPARE(static_cast<int>(ops.size()), 1);
        QCOMPARE(ops[0].type, collab::Operation::Type::Insert);
        QCOMPARE(ops[0].position, 0u);
        QCOMPARE(ops[0].text, std::string("X"));
    }

    void resetContentIsSilent() {
        EditorWidget w;
        w.setIdentity(1, 0);

        QSignalSpy spy(&w, &EditorWidget::localOperationsGenerated);
        w.resetContent(QStringLiteral("Hello"));

        QCOMPARE(spy.count(), 0);
        QCOMPARE(w.toPlainText(), QStringLiteral("Hello"));
    }

    void applyRemoteOperationDoesNotEchoBack() {
        EditorWidget w;
        w.setIdentity(1, 0);
        w.resetContent(QStringLiteral("abc"));

        QSignalSpy spy(&w, &EditorWidget::localOperationsGenerated);
        w.applyRemoteOperation(collab::make_insert(1, "XY", 2, 0));

        QCOMPARE(spy.count(), 0);
        QCOMPARE(w.toPlainText(), QStringLiteral("aXYbc"));
    }

    void cyrillicInsertReportsUtf8ByteOffset() {
        EditorWidget w;
        w.setIdentity(1, 0);
        w.resetContent(QString::fromUtf8("Пр"));

        QSignalSpy spy(&w, &EditorWidget::localOperationsGenerated);
        auto c = w.textCursor();
        c.movePosition(QTextCursor::End);
        w.setTextCursor(c);
        QTest::keyClick(&w, 'X');

        QCOMPARE(spy.count(), 1);
        auto ops = spy.first().at(0).value<std::vector<collab::Operation>>();
        QCOMPARE(static_cast<int>(ops.size()), 1);
        QCOMPARE(ops[0].type, collab::Operation::Type::Insert);
        QCOMPARE(ops[0].position, 4u);
        QCOMPARE(ops[0].text, std::string("X"));
    }

    void replaceSelectionProducesDeleteThenInsert() {
        EditorWidget w;
        w.setIdentity(1, 0);
        w.resetContent(QStringLiteral("abcde"));

        QSignalSpy spy(&w, &EditorWidget::localOperationsGenerated);

        auto c = w.textCursor();
        c.setPosition(1);
        c.setPosition(4, QTextCursor::KeepAnchor);
        w.setTextCursor(c);
        QTest::keyClick(&w, 'X');

        QCOMPARE(spy.count(), 1);
        auto ops = spy.first().at(0).value<std::vector<collab::Operation>>();
        QCOMPARE(static_cast<int>(ops.size()), 2);
        QCOMPARE(ops[0].type, collab::Operation::Type::Delete);
        QCOMPARE(ops[0].position, 1u);
        QCOMPARE(ops[0].length, 3u);
        QCOMPARE(ops[1].type, collab::Operation::Type::Insert);
        QCOMPARE(ops[1].position, 1u);
        QCOMPARE(ops[1].text, std::string("X"));
    }
};

QTEST_MAIN(EditorWidgetTest)
#include "test_editor_widget.moc"
