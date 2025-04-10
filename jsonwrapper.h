#pragma once

class JsonWrapper
{
public:
	JsonWrapper() = default;
	explicit JsonWrapper( QJsonDocument& doc ) : document_( std::move( doc ) ) {}

	[[nodiscard]] QJsonDocument& Document();
	[[nodiscard]] const QJsonDocument& Document() const;

	[[nodiscard]] QJsonValue Get( const QString& path ) const;
	bool Set( const QString& path, const QJsonValue& value );
	bool Remove( const QString& path );

	[[nodiscard]] QStringList GetPlayerNames() const;
	[[nodiscard]] QStringList GetPlayerIds() const;
	[[nodiscard]] QString GetPlayerName( const QString& steamId ) const;
	[[nodiscard]] QString GetPlayerId( const QString& playerName ) const;

	[[nodiscard]] bool IsNull() const;

	// Export JSON
	[[nodiscard]] QByteArray ToJson( QJsonDocument::JsonFormat format = QJsonDocument::Indented ) const;

private:
	QJsonDocument document_;
};

class PropertyPath : public QString
{
public:
	explicit PropertyPath( const QString& path ) : QString( path ) {}

	[[nodiscard]] QStringList ToList() const;

	[[nodiscard]] static PropertyPath PlayerPath();
	[[nodiscard]] static PropertyPath PlayerNamePath( const QString& steamId );
	[[nodiscard]] static PropertyPath PlayerUpgrade( const QString& steamId, const QString& upgradeName );
	[[nodiscard]] static PropertyPath RunStatsPath( const QString& key );
	[[nodiscard]] static PropertyPath TeamNamePath();
	[[nodiscard]] static PropertyPath DateAndTimePath();
	[[nodiscard]] static PropertyPath TimePlayedPath();
	[[nodiscard]] static PropertyPath ItemsPurchasedPath();
	[[nodiscard]] static PropertyPath ItemsPurchasedCountPath( const QString& itemName );

	[[nodiscard]] static QStringList ToList( const QString& path );
};
