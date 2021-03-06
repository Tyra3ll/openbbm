/*
  	This software and the content provided for use with it is Copyright © 2014-2020 Singular Sound 
 	BeatBuddy Manager is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as published by
    the Free Software Foundation.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <QHBoxLayout>
#include <QDebug>
#include <QPainter>
#include <QStyleOption>
#include <QLineEdit>

#include "songpartwidget.h"
#include "partcolumnwidget.h"
#include "../stylesheethelper.h"
#include "movehandlewidget.h"
#include "deletehandlewidget.h"
#include "../model/tree/abstracttreeitem.h"
#include "../player/songPlayer.h"
#include "../model/tree/project/beatsprojectmodel.h"
#include "../dialogs/loopCountDialog.h"
#include <QSettings>

SongPartWidget::SongPartWidget(BeatsProjectModel *p_Model, QWidget *parent) :
   SongFolderViewItem(p_Model, parent)
{
   // Data

   m_intro = false;
   m_outro = false;

   m_selectedStyleSheet = STYLE_NONE;
   mp_PartColumnItems = new QList<PartColumnWidget*>();
   // 4 main components
   mp_ContentPanel = new QWidget(this);
   mp_MoveHandleWidget = new MoveHandleWidget(this);
   mp_MoveHandleWidget->setMinimumWidth(mp_MoveHandleWidget->PREFERRED_WIDTH);
   mp_MoveHandleWidget->setMaximumWidth(mp_MoveHandleWidget->PREFERRED_WIDTH);
   mp_DeleteHandleWidget = new DeleteHandleWidget(this,0);
   mp_DeleteHandleWidget->setMinimumWidth(mp_DeleteHandleWidget->PREFERRED_WIDTH);
   mp_DeleteHandleWidget->setMaximumWidth(mp_DeleteHandleWidget->PREFERRED_WIDTH);
   mp_Title = new QLineEdit(this);
   mp_Title->setObjectName(QStringLiteral("titleEdit"));
   mp_Title->setMinimumHeight(20);
   mp_Title->setToolTip(tr("Set the name of the song part"));
   mp_Title->setStyleSheet("#titleEdit { background-color: transparent;  qproperty-frame: false ; }");

   mp_LoopCount = new LoopCountDialog(this);
   mp_LoopCount->setMinimumWidth(mp_DeleteHandleWidget->PREFERRED_WIDTH);
   mp_LoopCount->setMaximumWidth(mp_DeleteHandleWidget->PREFERRED_WIDTH);

   // connect slots to signals
   connect(this, SIGNAL(sigIsFirst(bool)), mp_MoveHandleWidget  , SLOT(slotIsFirst   (bool)));
   connect(this, SIGNAL(sigIsLast (bool)), mp_MoveHandleWidget  , SLOT(slotIsLast    (bool)));
   connect(this, SIGNAL(sigIsAlone(bool)), mp_DeleteHandleWidget, SLOT(slotIsDisabled(bool)));

   connect(mp_MoveHandleWidget, SIGNAL(sigSubWidgetClicked()), this, SLOT(slotSubWidgetClicked()));
   connect(mp_MoveHandleWidget, SIGNAL(sigUpClicked()), this, SLOT(slotMovePartUpClicked()));
   connect(mp_MoveHandleWidget, SIGNAL(sigDownClicked()), this, SLOT(slotMovePartDownClicked()));
   connect(mp_DeleteHandleWidget, SIGNAL(sigDeleteClicked()), this, SLOT(deleteButtonClicked()));
   connect(mp_DeleteHandleWidget, SIGNAL(sigSubWidgetClicked()), this, SLOT(slotSubWidgetClicked()));
   connect(mp_LoopCount, SIGNAL(sigSetLoopCount()), this, SLOT(slotLoopCountEntered()));
   connect(mp_LoopCount, SIGNAL(sigSubWidgetClicked()), this, SLOT(slotSubWidgetClicked()));
   connect(mp_Title    , SIGNAL(editingFinished()), this,         SLOT(slotTitleChangeByUI (   )));
   connect(mp_Title    , SIGNAL(selectionChanged()), this,         SLOT(slotTitleChangeByUI (   )));
   connect(mp_Title    , SIGNAL(textChanged(QString)), this,         SLOT(slotTitleChangeByUI (   )));
   connect(mp_Title    , SIGNAL(textEdited(QString)), this,         SLOT(slotTitleChangeByUI (   )));
}

// Required to apply stylesheet
void SongPartWidget::paintEvent(QPaintEvent * /*event*/)
{
   QStyleOption opt;
   opt.init(this);
   QPainter p(this);
   style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SongPartWidget::populate(QModelIndex const& modelIndex)
{
   // Delete all previous children widgets (if any)
   for(int i = 0; i < mp_ChildrenItems->size(); i++){
      // TODO verify if needs to hide before deleting
      delete mp_ChildrenItems->at(i);
   }

   mp_ChildrenItems->clear();

   // copy new model index
   setModelIndex(modelIndex);

   // Populate self's data

   // Add all items from model
   // For now, we only read the immediate children
   PartColumnWidget *p_PartColumnWidget = nullptr;

   for(int i = 0; i < modelIndex.model()->rowCount(modelIndex); i++){
      QModelIndex childIndex = modelIndex.model()->index(i, 0, modelIndex);  // NOTE: Widgets are associated to column 0
      if (childIndex.isValid()){
         p_PartColumnWidget = new PartColumnWidget(model(), mp_ContentPanel);
         p_PartColumnWidget->populate(childIndex);
         mp_ChildrenItems->append(p_PartColumnWidget);
         mp_PartColumnItems->append(p_PartColumnWidget);
         if(mp_PartColumnItems->size() > 1){
            updateTransMain();//only enter on part # 2 for trans fill
         }
         connect(p_PartColumnWidget, SIGNAL(sigSubWidgetClicked(QModelIndex)), this, SLOT(slotSubWidgetClicked(QModelIndex)));
         connect(p_PartColumnWidget, &PartColumnWidget::sigSelectTrack, this, &SongPartWidget::slotSelectTrack);
         connect(p_PartColumnWidget, &PartColumnWidget::sigUpdateTran, this, &SongPartWidget::updateTransMain);
         connect(p_PartColumnWidget, &PartColumnWidget::sigRowInserted, this, &SongPartWidget::parentAPBoxStatusChanged);
         connect(p_PartColumnWidget, &PartColumnWidget::sigRowDeleted, this, &SongPartWidget::updateOnDeletedChild);
      }
   }

   if(m_intro){
      mp_MoveHandleWidget->setLabelText(tr("I", "Very narrow field - acronym for \"Intro\""));
      mp_MoveHandleWidget->hideArrows();
      mp_DeleteHandleWidget->hideButton();
      mp_DeleteHandleWidget->setVisible((false));
      mp_LoopCount->setVisible(false);
      mp_Title->setVisible(false);
   } else if (m_outro){
      mp_MoveHandleWidget->setLabelText(tr("O", "Very narrow field - acronym for \"Outro\""));
      mp_MoveHandleWidget->hideArrows();
      mp_DeleteHandleWidget->hideButton();
      mp_DeleteHandleWidget->setVisible((false));
      mp_LoopCount->setVisible(false);
      mp_Title->setVisible(false);
   } else {
      mp_MoveHandleWidget->setLabelText(QString::number(modelIndex.row()));

      if (p_PartColumnWidget){
         p_PartColumnWidget->changeToolTip(tr("Add wav file"));
      }
      
      mp_LoopCount->setLoopCount(modelIndex.sibling(modelIndex.row(), AbstractTreeItem::LOOP_COUNT).data().toInt());
      // Disable autopilot by hiding Loop button
      mp_LoopCount->setVisible(true);
   }
   partName = "Part "+QString::number(modelIndex.row());
   mp_Title->setText(partName);
   m_playingInternal = modelIndex.sibling(modelIndex.row(), AbstractTreeItem::PLAYING).data().toBool();
   m_validInternal   = modelIndex.sibling(modelIndex.row(), AbstractTreeItem::INVALID).data().toString().isEmpty();

   // priority to validity
   if(!m_validInternal){
     
      // Avoid setting the stylesheet twice the same
      // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
      if(m_selectedStyleSheet != STYLE_INVALID){
         m_selectedStyleSheet = STYLE_INVALID;
         setStyleSheet("SongPartWidget QLabel{"
                       "color: rgb(255, 0, 0);}");

      }
   } else if (m_playingInternal){
      // Avoid setting the stylesheet twice the same
      // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
      if(m_selectedStyleSheet != STYLE_PLAYING){
         m_selectedStyleSheet = STYLE_PLAYING;
         setStyleSheet("SongPartWidget{"
                       "background: rgb(146, 176, 204);}");
      }
   } else {
      // Avoid setting the stylesheet twice the same
      // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
      if(m_selectedStyleSheet != STYLE_NONE){
         m_selectedStyleSheet = STYLE_NONE;
         setStyleSheet(nullptr);
      }
   }
}

void SongPartWidget::updateMinimumSize()
{
   // Recursively update all children first
   for(int i = 0; i < mp_ChildrenItems->size(); i++) {
      mp_ChildrenItems->at(i)->updateMinimumSize();
   }

   int width = mp_MoveHandleWidget->minimumWidth() +
         mp_DeleteHandleWidget->minimumWidth();

   for(int i = 0; i < mp_ChildrenItems->size(); i++){
      width += mp_ChildrenItems->at(i)->minimumWidth();
   }
   setMinimumWidth(width);

   // Second colomn is the one that is potentially the highest
   if(mp_ChildrenItems->size() >= 2){
      setMinimumHeight(mp_ChildrenItems->at(1)->minimumHeight()+2);
   }
}

void SongPartWidget::updateLayout()
{

   const int variableWidth = width() - mp_MoveHandleWidget->minimumWidth() - mp_DeleteHandleWidget->minimumWidth();
   const int variableMinWidth = minimumWidth() - mp_MoveHandleWidget->minimumWidth() - mp_DeleteHandleWidget->minimumWidth();

   // Relative to this
   // Note: keep some place for insert buttons
   QPoint drawCursor(0,12);
   if(m_intro){
      drawCursor.setY(1);
      mp_MoveHandleWidget->setGeometry( QRect(drawCursor, QSize(mp_MoveHandleWidget->minimumWidth(), height()-13)) );
   } else if(m_outro){
      mp_MoveHandleWidget->setGeometry( QRect(drawCursor, QSize(mp_MoveHandleWidget->minimumWidth(), height()-13)) );
   } else {
      mp_MoveHandleWidget->setGeometry( QRect(drawCursor, QSize(mp_MoveHandleWidget->minimumWidth(), height()-24)) );
   }
   drawCursor.rx() += mp_MoveHandleWidget->minimumWidth();

   // Note: 1 px removed on top and bottom for separator line
   drawCursor.setY(1);
   mp_ContentPanel->setGeometry( QRect(drawCursor, QSize(variableWidth, height()-2)) );
   drawCursor.rx() += variableWidth;

   mp_DeleteHandleWidget->setGeometry( QRect(drawCursor, QSize(mp_DeleteHandleWidget->minimumWidth(), height()/2-2)) );
   drawCursor.setY(height()/2);
   mp_LoopCount->setGeometry( QRect(drawCursor, QSize(mp_DeleteHandleWidget->minimumWidth(), height()/2-2)) );



   // Drawing is relative to mp_ContentPanel
   // Note, we work with coordinates that are not devided to avoid cumulative rounding errors
   int xPositionMult = 0;

   for(int i = 0; i < mp_ChildrenItems->size(); i++){

      int nextXPositionMult = xPositionMult + (mp_ChildrenItems->at(i)->minimumWidth() * variableWidth);

      mp_ChildrenItems->at(i)->setGeometry(QRect(QPoint(xPositionMult/variableMinWidth, 0), QPoint(nextXPositionMult/variableMinWidth - 1, height() - 2)));
      mp_ChildrenItems->at(i)->updateLayout();
      xPositionMult = nextXPositionMult;
   }
   //Part Name
   mp_Title->setAlignment(Qt::AlignRight);
}

void SongPartWidget::dataChanged(const QModelIndex &left, const QModelIndex &right)
{
   if(!left.isValid()){
      return;
   }

   for(int column = left.column(); column <= right.column(); column++){
      QModelIndex index = left.sibling(left.row(), column);
      if(!index.isValid()){
         continue;
      }

      switch(column){
         case AbstractTreeItem::PLAYING:
            m_playingInternal = index.data().toBool();
            // priority to validity
            if(!m_validInternal){
               // NO CHANGE
            } else if (m_playingInternal){
               // Avoid setting the stylesheet twice the same
               // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
               if(m_selectedStyleSheet != STYLE_PLAYING){
                  m_selectedStyleSheet = STYLE_PLAYING;
                  setStyleSheet("SongPartWidget{"
                                "background: rgb(146, 176, 204);}");
               }
            } else {
               // Avoid setting the stylesheet twice the same
               // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
               if(m_selectedStyleSheet != STYLE_NONE){
                  m_selectedStyleSheet = STYLE_NONE;
                  setStyleSheet(nullptr);
               }
            }
            break;

         case AbstractTreeItem::INVALID:
            m_validInternal = index.data().toString().isEmpty();
            // priority to validity
            if(!m_validInternal){

               // Avoid setting the stylesheet twice the same
               // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
               if(m_selectedStyleSheet != STYLE_INVALID){
                  m_selectedStyleSheet = STYLE_INVALID;
                  setStyleSheet("SongPartWidget QLabel{"
                                "color: rgb(255, 0, 0);}");
               }
            } else if (m_playingInternal){
               // Avoid setting the stylesheet twice the same
               // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
               if(m_selectedStyleSheet != STYLE_PLAYING){
                  m_selectedStyleSheet = STYLE_PLAYING;
                  setStyleSheet("SongPartWidget{"
                                "background: rgb(146, 176, 204);}");
               }
            } else {
               // Avoid setting the stylesheet twice the same
               // watch out for bug https://bugreports.qt-project.org/browse/QTBUG-20292
               if(m_selectedStyleSheet != STYLE_NONE){
                  m_selectedStyleSheet = STYLE_NONE;
                  setStyleSheet(nullptr);
               }
            }
            break;
         default:
            // Do nothing
            break;
      }
   }
}


void SongPartWidget::setIntro(bool intro)
{
   m_intro = intro;

   // Intro and outro are exclusive
   if(intro){
      m_outro = false;
   }
}
bool SongPartWidget::isIntro() const
{
   return m_intro;
}

void SongPartWidget::setOutro(bool outro)
{
   m_outro = outro;

   // Intro and outro are exclusive
   if(outro){
      m_intro = false;
   }

}
bool SongPartWidget::isOutro() const
{
   return m_outro;
}

int SongPartWidget::headerColumnWidth(int columnIndex)
{
   if (columnIndex == 0){
      return mp_MoveHandleWidget->width();
   }
   if (columnIndex == 5){
      return mp_DeleteHandleWidget->width();
   }

   if(mp_ChildrenItems->count() < columnIndex){
      return 0;
   }

   return mp_ChildrenItems->at(columnIndex - 1)->width();
}

void SongPartWidget::deleteButtonClicked()
{
    model()->deleteSongPart(modelIndex());
}


void SongPartWidget::slotIsFirst(bool first)
{
   emit sigIsFirst(first);
}

void SongPartWidget::slotIsLast(bool last)
{
   emit sigIsLast(last);
}

void SongPartWidget::slotIsAlone(bool alone)
{
   emit sigIsAlone(alone);
}

void SongPartWidget::slotOrderChanged(int number)
{
   if(isIntro() || isOutro()){
      return;
   }
   mp_MoveHandleWidget->setLabelText(QString::number(number));
}

void SongPartWidget::slotMovePartUpClicked()
{
    model()->moveItem(modelIndex(), -1);
}

void SongPartWidget::slotMovePartDownClicked()
{
    model()->moveItem(modelIndex(), 1);
}

void SongPartWidget::slotSelectTrack(const QByteArray &trackData, int trackIndex, int typeId)
{
   // superseed type id in case of intro/outro
   if(m_intro){
      typeId = INTR_FILL_ID;
   } else if(m_outro){
      typeId = OUTR_FILL_ID;
   }

   emit sigSelectTrack(trackData, trackIndex, typeId, modelIndex().row());
}

void SongPartWidget::slotLoopCountEntered()
{

    model()->setData(modelIndex().sibling(modelIndex().row(), AbstractTreeItem::LOOP_COUNT), mp_LoopCount->getLoopCount());
}

void SongPartWidget::slotTitleChangeByUI()
{
    if(mp_Title->text() != partName){
        qDebug() << "Part Name Entered:" << mp_Title->text();
        //model()->setData(modelIndex().sibling(modelIndex().row(), AbstractTreeItem::PART_NAME), mp_Title->text());
    }
}

void SongPartWidget::parentAPBoxStatusChanged()
{
    int sigNum = mp_PartColumnItems->at(0)->getNumSignature();
    if(sigNum >0){//if zero means part is empty
        for(int i = 0; i < mp_PartColumnItems->size();i++){
            mp_PartColumnItems->at(i)->parentAPBoxStatusChanged(sigNum);
        }
    }
}
void SongPartWidget::updateOnDeletedChild(int type){
    //only update  unchanged parts means if the change was on main fill main fill does not gets updated, if trans fill, transfill does not get updated
    int sigNum = mp_PartColumnItems->at(0)->getNumSignature();
    for(int i = 0; i < mp_PartColumnItems->size();i++){
        if(i != type){
          mp_PartColumnItems->at(i)->parentAPBoxStatusChanged(sigNum);
          if(type == 2){
              //if the transfill was removed main fill should update
              mp_PartColumnItems->at(i)->updateAPText(false,false,i);
          }
        }
    }
}

void SongPartWidget::updateTransMain(){

    bool finiteMain = false;
    //fill variable with true if transfill should be additional bars
    if(!m_intro && !m_outro){
        finiteMain = mp_PartColumnItems->at(0)->finitePart();
        if(mp_PartColumnItems->size() > 0){
           int idx = (mp_PartColumnItems->size() > 3)?2:mp_PartColumnItems->size() - 1;
           mp_PartColumnItems->at(idx)->updateAPText(false,finiteMain,0);//the last number does not matter here
        }

    }
}
