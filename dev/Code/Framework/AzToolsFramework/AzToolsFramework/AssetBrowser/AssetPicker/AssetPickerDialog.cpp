/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/UserSettings/UserSettings.h>

#include <AzQtComponents/Components/DockBar.h>

#include <AzToolsFramework/AssetBrowser/Views/AssetBrowserTreeView.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserFilterModel.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserModel.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>
#include <AzToolsFramework/AssetBrowser/AssetSelectionModel.h>
#include <AzToolsFramework/UI/UICore/QWidgetSavedState.h>
#include <AzToolsFramework/AssetBrowser/AssetPicker/AssetPickerDialog.h>

AZ_PUSH_DISABLE_WARNING(4251 4244, "-Wunknown-warning-option") // disable warnings spawned by QT
#include "AssetBrowser/AssetPicker/ui_AssetPickerDialog.h"
#include <QPushButton>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QTimer>
AZ_POP_DISABLE_WARNING

namespace AzToolsFramework
{
    namespace AssetBrowser
    {
        AssetPickerDialog::AssetPickerDialog(AssetSelectionModel& selection, QWidget* parent)
            : QDialog(parent)
            , m_ui(new Ui::AssetPickerDialogClass())
            , m_filterModel(new AssetBrowserFilterModel(parent))
            , m_selection(selection)
            , m_hasFilter(false)
        {
            m_filterStateSaver = AzToolsFramework::TreeViewState::CreateTreeViewState();

            m_ui->setupUi(this);
            m_ui->m_searchWidget->Setup(true, false);

            m_ui->m_searchWidget->GetFilter()->AddFilter(m_selection.GetDisplayFilter());

            using namespace AzToolsFramework::AssetBrowser;
            AssetBrowserComponentRequestBus::BroadcastResult(m_assetBrowserModel, &AssetBrowserComponentRequests::GetAssetBrowserModel);
            AZ_Assert(m_assetBrowserModel, "Failed to get asset browser model");
            m_filterModel->setSourceModel(m_assetBrowserModel);
            m_filterModel->SetFilter(m_ui->m_searchWidget->GetFilter());

            QString name = m_selection.GetDisplayFilter()->GetName();

            m_ui->m_assetBrowserTreeViewWidget->setModel(m_filterModel.data());
            m_ui->m_assetBrowserTreeViewWidget->SetThumbnailContext("AssetBrowser");
            m_ui->m_assetBrowserTreeViewWidget->setSelectionMode(selection.GetMultiselect() ?
                QAbstractItemView::SelectionMode::ExtendedSelection : QAbstractItemView::SelectionMode::SingleSelection);
            m_ui->m_assetBrowserTreeViewWidget->setDragEnabled(false);

            // if the current selection is invalid, disable the Ok button
            m_ui->m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(EvaluateSelection());
            m_ui->m_buttonBox->button(QDialogButtonBox::Ok)->setProperty("class", "Primary");
            m_ui->m_buttonBox->button(QDialogButtonBox::Cancel)->setProperty("class", "Secondary");

            connect(m_ui->m_searchWidget->GetFilter().data(), &AssetBrowserEntryFilter::updatedSignal, m_filterModel.data(), &AssetBrowserFilterModel::filterUpdatedSlot);
            connect(m_filterModel.data(), &AssetBrowserFilterModel::filterChanged, this, [this]()
            {
                const bool hasFilter = !m_ui->m_searchWidget->GetFilterString().isEmpty();
                const bool selectFirstFilteredIndex = true;
                m_ui->m_assetBrowserTreeViewWidget->UpdateAfterFilter(hasFilter, selectFirstFilteredIndex);
            });
            connect(m_ui->m_assetBrowserTreeViewWidget, &QAbstractItemView::doubleClicked, this, &AssetPickerDialog::DoubleClickedSlot);
            connect(m_ui->m_assetBrowserTreeViewWidget, &AssetBrowserTreeView::selectionChangedSignal, this,
                [this](const QItemSelection&, const QItemSelection&){ AssetPickerDialog::SelectionChangedSlot(); });
            connect(m_ui->m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(m_ui->m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

            m_ui->m_assetBrowserTreeViewWidget->LoadState("AssetBrowserTreeView_" + name);

            for (auto& assetId : selection.GetSelectedAssetIds())
            {
                m_ui->m_assetBrowserTreeViewWidget->SelectProduct(assetId);
            }

            setWindowTitle(tr("Pick %1").arg(name));

            m_persistentState = AZ::UserSettings::CreateFind<AzToolsFramework::QWidgetSavedState>(AZ::Crc32(("AssetBrowserTreeView_Dialog_" + name).toUtf8().data()), AZ::UserSettings::CT_GLOBAL);

            QTimer::singleShot(0, this, &AssetPickerDialog::RestoreState);
            SelectionChangedSlot();
        }

        AssetPickerDialog::~AssetPickerDialog() = default;

        void AssetPickerDialog::accept()
        {
            SaveState();
            QDialog::accept();
        }

        void AssetPickerDialog::reject()
        {
            m_selection.GetResults().clear();
            SaveState();
            QDialog::reject();
        }

        void AssetPickerDialog::OnFilterUpdated()
        {
            if (!m_hasFilter)
            {
                m_filterStateSaver->CaptureSnapshot(m_ui->m_assetBrowserTreeViewWidget);
            }

            m_filterModel->filterUpdatedSlot();

            const bool hasFilter = m_ui->m_searchWidget->hasStringFilter();

            if (hasFilter)
            {
                // The update slot queues the update, so we need to react after that update.
                QTimer::singleShot(0, this, [this]()
                {
                    m_ui->m_assetBrowserTreeViewWidget->expandAll();
                });
            }

            if (m_hasFilter && !hasFilter)
            {
                m_filterStateSaver->ApplySnapshot(m_ui->m_assetBrowserTreeViewWidget);
                m_hasFilter = false;
            }
            else if (!m_hasFilter && hasFilter)
            {
                m_hasFilter = true;
            }
        }

        void AssetPickerDialog::keyPressEvent(QKeyEvent* e)
        {
            // Until search widget is revised, Return key should not close the dialog,
            // it is used in search widget interaction
            if (e->key() == Qt::Key_Return)
            {
                if (EvaluateSelection())
                {
                    QDialog::accept();
                }
            }
            else
            {
                QDialog::keyPressEvent(e);
            }
        }

        bool AssetPickerDialog::EvaluateSelection() const
        {
            auto selectedAssets = m_ui->m_assetBrowserTreeViewWidget->GetSelectedAssets();
            // exactly one item must be selected, even if multi-select option is disabled, still good practice to check
            if (selectedAssets.empty())
            {
                return false;
            }

            m_selection.GetResults().clear();

            for (auto entry : selectedAssets)
            {
                m_selection.GetSelectionFilter()->Filter(m_selection.GetResults(), entry);
                if (m_selection.IsValid() && !m_selection.GetMultiselect())
                {
                    break;
                }
            }
            return m_selection.IsValid();
        }

        void AssetPickerDialog::DoubleClickedSlot(const QModelIndex& index)
        {
            AZ_UNUSED(index);
            if (EvaluateSelection())
            {
                QDialog::accept();
            }
        }

        void AssetPickerDialog::UpdatePreview() const
        {
            auto selectedAssets = m_ui->m_assetBrowserTreeViewWidget->GetSelectedAssets();
            if (selectedAssets.size() != 1)
            {
                m_ui->m_previewerFrame->Clear();
                return;
            }

            m_ui->m_previewerFrame->Display(selectedAssets.front());
        }

        void AssetPickerDialog::SaveState()
        {
            m_ui->m_assetBrowserTreeViewWidget->SaveState();
            if (m_persistentState)
            {
                m_persistentState->CaptureGeometry(parentWidget() ? parentWidget() : this);
            }
        }

        void AssetPickerDialog::RestoreState()
        {
            if (m_persistentState)
            {
                const auto widget = parentWidget() ? parentWidget() : this;
                m_persistentState->RestoreGeometry(widget);
            }
        }

        void AssetPickerDialog::SelectionChangedSlot()
        {
            m_ui->m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(EvaluateSelection());
            UpdatePreview();
        }
    } // AssetBrowser
} // AzToolsFramework

#include <AssetBrowser/AssetPicker/AssetPickerDialog.moc>
