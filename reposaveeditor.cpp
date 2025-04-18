﻿#include "stdafx.h"
#include "reposaveeditor.h"
#include "playereditionwindow.h"

#include <cryptopp/modes.h>
#include <cryptopp/gzip.h>
#include <cryptopp/osrng.h>
#include <fstream>

RepoSaveEditor::RepoSaveEditor( QWidget* parent ) : QMainWindow( parent ), ui( new Ui::RepoSaveEditorClass() )
{
	ui->setupUi( this );

	setWindowTitle( QString( "%1 - %2" ).arg( windowTitle() ).arg( Version ) );

	connect( ui->actionOpen, &QAction::triggered, this, &RepoSaveEditor::OpenFile );
	connect( ui->actionSave, &QAction::triggered, this, &RepoSaveEditor::SaveOpenedFile );
	connect( ui->actionSaveAs, &QAction::triggered, this, &RepoSaveEditor::SaveFileAs );

	connect( ui->actionAddPlayer, &QAction::triggered, this, &RepoSaveEditor::AddPlayer );
	connect( ui->actionRemovePlayer, &QAction::triggered, this, &RepoSaveEditor::RemovePlayer );

	connect( ui->worldWidget, &WorldWidget::Edited, this, &RepoSaveEditor::UpdateJsonText );
	connect( ui->itemWidget, &ItemWidget::Edited, this, &RepoSaveEditor::UpdateJsonText );
	connect( ui->playerWidget, &PlayerWidget::Edited, this, &RepoSaveEditor::UpdateJsonText );

	connect( ui->checkJsonButton, &QPushButton::clicked, this, &RepoSaveEditor::CheckJson );

	ui->advancedTextEdit->setReadOnly( !ui->editableCheckBox->isChecked() );
	connect( ui->editableCheckBox, &QCheckBox::checkStateChanged, this, [this]
	{
		ui->advancedTextEdit->setReadOnly( !ui->editableCheckBox->isChecked() );
	} );

	jsonHighlighter = new JsonHighlighter( ui->advancedTextEdit->document() );

	HideUi();
	SetupShortcuts();

#if defined( _DEBUG )
	if ( QDirIterator it( DEFAULT_SAVES_LOCATION, QStringList() << "*.es3", QDir::Files, QDirIterator::Subdirectories ); it.hasNext() )
	{
		openedFile = it.next();
		LoadJson( DecryptFile( openedFile ) );
	}
#endif
}

RepoSaveEditor::~RepoSaveEditor()
{
	delete ui;
}

void RepoSaveEditor::SetWidgetsVisible( const QLayout* layout, const bool enabled )
{
	for ( int i = 0; i < layout->count(); ++i )
	{
		QLayoutItem* item = layout->itemAt( i );

		if ( const auto widget = item->widget() )
		{
			widget->setVisible( enabled );
		}
		else if ( const auto childLayout = item->layout() )
		{
			SetWidgetsVisible( childLayout, enabled );
		}
	}
}

void RepoSaveEditor::OpenFile()
{
	openedFile = QFileDialog::getOpenFileName( this, "Open a File", DEFAULT_SAVES_LOCATION, "ES3 File (*.es3)" );

	if ( openedFile.isEmpty() )
		return;

	HideUi();

	const QString json = DecryptFile( openedFile );

	if ( json.isEmpty() )
	{
		QMessageBox::critical( this, "Error", "Error while decrypting data." );
		qCritical() << "Error : empty decrypted data.";
		return;
	}

	LoadJson( json );
}

void RepoSaveEditor::SaveFile( const QString& filePath )
{
	// There is nothing to save if file is not open
	if ( filePath.isEmpty() )
		return;

	// If directory with save file was deleted, create directory again and save file there
	const QString directoryPath = QFileInfo( filePath ).absolutePath();

	if ( const QDir dir; !dir.exists( directoryPath ) )
	{
		if ( !dir.mkpath( directoryPath ) )
		{
			QMessageBox::critical( this, "Error", "Unable to open file for writing." );
			qCritical() << "Error : Unable to open file for writing :" << filePath;
			return;
		}
	}

	QFile file( filePath );
	if ( !file.open( QIODevice::WriteOnly ) )
	{
		QMessageBox::critical( this, "Error", "Unable to open file for writing." );
		qCritical() << "Error : Unable to open file for writing :" << filePath;
		return;
	}

	// Récupère le texte du plainTextEdit
	const QString texte = ui->advancedTextEdit->toPlainText();

	// Vérification que le texte est un JSON valide
	QJsonParseError parseError;
	const QJsonDocument jsonDoc = QJsonDocument::fromJson( texte.toUtf8(), &parseError );

	if ( parseError.error != QJsonParseError::NoError )
	{
		QMessageBox::critical( this, "Error JSON", QString( "Error JSON : %1" ).arg( parseError.errorString() ) );
		qCritical() << "Error JSON :" << parseError.errorString();
		return;
	}

	// Formatte le JSON indenté avant de l'encrypter
	const QByteArray jsonFormatted = jsonDoc.toJson( QJsonDocument::Indented );

	// Encrypte les données JSON
	const QByteArray encryptedData = EncryptData( QString::fromUtf8( jsonFormatted ) );

	// Vérification de l'encryption
	if ( encryptedData.isEmpty() )
	{
		QMessageBox::critical( this, "Error", "Encrypted data is empty." );
		qCritical() << "Error : Encrypted data is empty.";
		return;
	}

	// Écrit les données cryptées dans le fichier
	file.write( encryptedData );
	file.close();

	// Information utilisateur
	QMessageBox::information( this, "Save", QString( "Save successful : %1" ).arg( filePath ) );
	qDebug() << "Save successful :" << filePath;
}

void RepoSaveEditor::SaveOpenedFile()
{
	SaveFile( openedFile );
}

void RepoSaveEditor::SaveFileAs()
{
	QString savesLocation = DEFAULT_SAVES_LOCATION;

	if ( openedFile.isEmpty() )
		return;

	const QString openedDirectory = QFileInfo( openedFile ).absolutePath();
	if ( const QDir dir; dir.exists( openedDirectory ) )
	{
		savesLocation = openedDirectory;
	}

	const QString filePath = QFileDialog::getSaveFileName( this, "Save as", savesLocation, "ES3 File (*.es3)" );

	SaveFile( filePath );
}

void RepoSaveEditor::AddPlayer()
{
	if ( json.IsNull() )
		return;

	auto* window = new PlayerEditionWindow( this, json );
	window->show();
	window->SetEditionMode( PlayerEditionWindow::ePlayerEditionMode::Add );
	connect( window, &PlayerEditionWindow::Edited, this, &RepoSaveEditor::UpdateWidgets );
}

void RepoSaveEditor::RemovePlayer()
{
	if ( json.IsNull() )
		return;

	auto* window = new PlayerEditionWindow( this, json );
	window->show();
	window->SetEditionMode( PlayerEditionWindow::ePlayerEditionMode::Remove );
	connect( window, &PlayerEditionWindow::Edited, this, &RepoSaveEditor::UpdateWidgets );
}

void RepoSaveEditor::LoadJson( const QString& filePath )
{
	QJsonParseError parseError;
	json = JsonWrapper( QJsonDocument::fromJson( filePath.toUtf8(), &parseError ) );
	if ( parseError.error != QJsonParseError::NoError )
	{
		QMessageBox::critical( this, "Error", "Invalid JSON." );
		qCritical() << "Error : invalid JSON.";
		return;
	}

	UpdateWidgets();
}

void RepoSaveEditor::UpdateWidgets() const
{
	ui->worldWidget->UpdateWidgets( json );
	ui->itemWidget->UpdateWidgets( json );
	ui->playerWidget->UpdateWidgets( json );
	ui->advancedTextEdit->setPlainText( json.ToJson() );
}

void RepoSaveEditor::UpdateJsonText()
{
	// Update JSON from widgets
	ui->worldWidget->SetJsonValue( json );
	ui->itemWidget->SetJsonValue( json );
	ui->playerWidget->SetJsonValue( json );

	// Update JSON text
	ui->advancedTextEdit->setPlainText( json.ToJson() );
}

void RepoSaveEditor::CheckJson()
{
	const QString jsonText = ui->advancedTextEdit->toPlainText();
	QJsonParseError parseError;
	const QJsonDocument jsonDoc = QJsonDocument::fromJson( jsonText.toUtf8(), &parseError );
	if ( parseError.error != QJsonParseError::NoError )
	{
		QMessageBox::critical( this, "Error JSON", QString( "Error JSON : %1" ).arg( parseError.errorString() ) );
		qCritical() << "Error JSON :" << parseError.errorString();
		return;
	}

	QMessageBox::information( this, "JSON", "JSON is valid." );
	qDebug() << "JSON is valid.";

	json = JsonWrapper( jsonDoc );

	ui->worldWidget->UpdateWidgets( json );
	ui->itemWidget->UpdateWidgets( json );
	ui->playerWidget->UpdateWidgets( json );
}

void RepoSaveEditor::HideUi() const
{
	ui->worldWidget->SetVisible( false );
	ui->itemWidget->SetVisible( false );
	ui->playerWidget->SetVisible( false );

	ui->advancedTextEdit->setPlainText( "" );
}

void RepoSaveEditor::SetupShortcuts() const
{
	// Setup shortcuts for actions
	ui->actionOpen->setShortcut( QKeySequence::Open );
	ui->actionSave->setShortcut( QKeySequence::Save );
	ui->actionSaveAs->setShortcut( QKeySequence::SaveAs );
}

QString RepoSaveEditor::DecryptFile( const QString& filePath )
{
	using namespace CryptoPP;

	QFile file( filePath );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		qCritical() << "Error opening file for reading.";
		return {};
	}

	if ( file.size() < 32 )
	{
		qCritical() << "File is too small to be a valid ES3 file.";
		return {};
	}

	QByteArray iv = file.read( 16 );
	QByteArray encryptedData = file.readAll();

	// Derived key
	byte key[ 16 ];
	DeriveKey( Password.toStdString(), reinterpret_cast < const byte* >( iv.data() ), key );

	// AES CBC decryption
	std::string decrypted_tmp;
	CBC_Mode < AES >::Decryption decryptor( key, sizeof( key ), reinterpret_cast < const byte* >( iv.data() ) );

	StringSource( reinterpret_cast < const byte* >( encryptedData.data() ), encryptedData.size(), true, new StreamTransformationFilter( decryptor, new StringSink( decrypted_tmp ), StreamTransformationFilter::PKCS_PADDING ) );

	// GZip decompression (optional)
	if ( decrypted_tmp.size() > 2 && static_cast < byte >( decrypted_tmp[ 0 ] ) == 0x1F && static_cast < byte >( decrypted_tmp[ 1 ] ) == 0x8B )
	{
		std::string decompressed;
		StringSource( decrypted_tmp, true, new Gunzip( new StringSink( decompressed ) ) );
		return QString::fromUtf8( decompressed.data(), static_cast < int >( decompressed.size() ) );
	}

	return QString::fromUtf8( decrypted_tmp.data(), static_cast < int >( decrypted_tmp.size() ) );
}

QByteArray RepoSaveEditor::EncryptData( const QString& data, const bool useGzip )
{
	using namespace CryptoPP;
	QByteArray rawData = data.toUtf8();

	// Generate IV
	byte iv[ 16 ];
	OS_GenerateRandomBlock( false, iv, sizeof( iv ) );

	// Derived key
	byte key[ 16 ];
	DeriveKey( Password.toStdString(), iv, key );

	std::string processedData;

	// GZip decompression (optional)
	if ( useGzip )
	{
		StringSource( reinterpret_cast < const byte* >( rawData.data() ), rawData.size(), true, new Gzip( new StringSink( processedData ) ) );
	}
	else
	{
		processedData.assign( rawData.constData(), rawData.size() );
	}

	// AES CBC encryption
	std::string encryptedData;
	CBC_Mode < AES >::Encryption encryptor( key, sizeof( key ), iv );

	StringSource( processedData, true, new StreamTransformationFilter( encryptor, new StringSink( encryptedData ), StreamTransformationFilter::PKCS_PADDING ) );

	// Prepend IV to encrypted data
	QByteArray result;
	result.append( reinterpret_cast < const char* >( iv ), sizeof( iv ) );
	result.append( encryptedData.data(), encryptedData.size() );

	return result;
}

void RepoSaveEditor::SaveData( const QJsonObject& jsonData, QWidget* parent )
{
	if ( jsonData.isEmpty() )
	{
		QMessageBox::critical( parent, "Erreur", "Aucune donnée à sauvegarder." );
		qCritical() << "Erreur : aucune donnée à sauvegarder.";
		return;
	}

	QString filePath = QFileDialog::getSaveFileName( parent, "Sauvegarder le fichier", QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ), "Fichier sauvegarde (*.es3)" );

	if ( !filePath.isEmpty() )
	{
		// Json conversion
		const QJsonDocument doc( jsonData );
		const QByteArray jsonFormatted = doc.toJson( QJsonDocument::Indented );

		// Json encryption
		const QByteArray encryptedData = EncryptData( QString::fromUtf8( jsonFormatted ) );

		// Write encrypted data to file
		QFile file( filePath );
		if ( file.open( QIODevice::WriteOnly ) )
		{
			file.write( encryptedData );
			file.close();

			QMessageBox::information( parent, "Sauvegarde réussie", QString( "Fichier sauvegardé : %1" ).arg( filePath ) );
			qDebug() << "Fichier sauvegardé :" << filePath;
		}
		else
		{
			QMessageBox::critical( parent, "Erreur", "Impossible d'écrire dans le fichier." );
			qCritical() << "Erreur ouverture fichier pour écriture :" << filePath;
		}
	}
	else
	{
		qCritical() << "Sauvegarde annulée par l'utilisateur.";
	}
}

void RepoSaveEditor::DeriveKey( const std::string& password, const CryptoPP::byte* iv, CryptoPP::byte* key )
{
	using namespace CryptoPP;

	constexpr int iterations = 100;
	constexpr int dkLen = 16;

	const size_t passwordLen = password.size();
	size_t saltLen = 16;

	HMAC < SHA1 > hmac( ( const byte* ) password.data(), passwordLen );
	byte buffer[ SHA1::DIGESTSIZE ];

	SecByteBlock derivedKey( dkLen );
	size_t counter = 1;
	size_t generated = 0;

	while ( generated < dkLen )
	{
		// Salt + counter
		byte salt_counter[ 20 ];
		memcpy( salt_counter, iv, saltLen );
		salt_counter[ saltLen ] = ( counter >> 24 ) & 0xff;
		salt_counter[ saltLen + 1 ] = ( counter >> 16 ) & 0xff;
		salt_counter[ saltLen + 2 ] = ( counter >> 8 ) & 0xff;
		salt_counter[ saltLen + 3 ] = counter & 0xff;

		// U1
		hmac.Restart();
		hmac.Update( salt_counter, saltLen + 4 );
		hmac.Final( buffer );

		byte u[ SHA1::DIGESTSIZE ];
		memcpy( u, buffer, SHA1::DIGESTSIZE );

		// XOR iterations
		for ( int i = 1; i < iterations; ++i )
		{
			hmac.Restart();
			hmac.Update( buffer, SHA1::DIGESTSIZE );
			hmac.Final( buffer );
			for ( size_t j = 0; j < SHA1::DIGESTSIZE; ++j )
				u[ j ] ^= buffer[ j ];
		}

		const size_t toCopy = std::min( dkLen - generated, static_cast < size_t >( SHA1::DIGESTSIZE ) );
		memcpy( derivedKey + generated, u, toCopy );

		generated += toCopy;
		++counter;
	}

	memcpy( key, derivedKey, dkLen );
}

void RepoSaveEditor::moveEvent( QMoveEvent* event )
{
	emit WindowMovedOrResized();
	QMainWindow::moveEvent( event );
}

void RepoSaveEditor::resizeEvent( QResizeEvent* event )
{
	emit WindowMovedOrResized();
	QMainWindow::resizeEvent( event );
}
