#include "stdafx.h"
#include "jsonwrapper.h"

QJsonDocument& JsonWrapper::Document()
{
	return document_;
}

const QJsonDocument& JsonWrapper::Document() const
{
	return document_;
}

QJsonValue JsonWrapper::Get( const QString& path ) const
{
	QStringList keys = PropertyPath::ToList( path );
	const QJsonObject obj = document_.object();
	QJsonValue val = obj;

	for ( const QString& key : keys )
	{
		if ( !val.isObject() )
			return {}; // Invalid path
		val = val.toObject().value( key );
	}
	return val;
}

bool JsonWrapper::Set( const QString& path, const QJsonValue& value )
{
	QStringList keys = PropertyPath::ToList( path );
	if ( keys.isEmpty() )
		return false;

	QJsonObject root = document_.object();
	QJsonObject* current = &root;

	QList < QJsonObject* > hierarchy; // for reconstitution

	// Descending the hierarchy
	for ( int i = 0; i < keys.size() - 1; ++i )
	{
		const QString& key = keys[ i ];
		const QJsonObject next = current->value( key ).toObject();
		hierarchy.append( current );
		current = new QJsonObject( next ); // new object to avoid modifying the original
	}

	// Insert value at the last level
	current->insert( keys.last(), value );

	// Rebuild the hierarchy
	for ( int i = static_cast < int >( keys.size() ) - 2; i >= 0; --i )
	{
		QJsonObject* parent = hierarchy[ i ];
		parent->insert( keys[ i ], *current );
		current = parent;
	}

	document_.setObject( root );
	return true;
}

bool JsonWrapper::Remove( const QString& path )
{
	QStringList keys = PropertyPath::ToList( path );
	if ( keys.isEmpty() )
		return false;

	QJsonObject root = document_.object();
	QJsonObject* current = &root;

	QList < QJsonObject* > hierarchy;

	// Descending the hierarchy
	for ( int i = 0; i < keys.size() - 1; ++i )
	{
		const QString& key = keys[ i ];
		if ( !current->contains( key ) || !( *current )[ key ].isObject() )
			return false;

		const QJsonObject next = ( *current )[ key ].toObject();
		hierarchy.append( current );
		current = new QJsonObject( next );
	}

	// Remove the last key
	current->remove( keys.last() );

	// Rebuild the hierarchy
	for ( int i = static_cast < int >( keys.size() ) - 2; i >= 0; --i )
	{
		QJsonObject* parent = hierarchy[ i ];
		parent->insert( keys[ i ], *current );
		current = parent;
	}

	document_.setObject( root );
	return true;
}

QStringList JsonWrapper::GetPlayerNames() const
{
	QStringList playerNames;
	for ( const QString& playerId : GetPlayerIds() )
	{
		if ( const QJsonValue playerName = Get( PropertyPath::PlayerNamePath( playerId ) ); playerName.isString() )
			playerNames.append( playerName.toString() );
	}
	return playerNames;
}

QStringList JsonWrapper::GetPlayerIds() const
{
	if ( const QJsonValue playerNames = Get( PropertyPath::PlayerPath() ); playerNames.isObject() )
	{
		return playerNames.toObject().keys();
	}
	return {};
}

QString JsonWrapper::GetPlayerName( const QString& steamId ) const
{
	if ( const QJsonValue playerName = Get( PropertyPath::PlayerNamePath( steamId ) ); playerName.isString() )
	{
		return playerName.toString();
	}
	return {};
}

QString JsonWrapper::GetPlayerId( const QString& playerName ) const
{
	if ( const QJsonValue playerId = Get( PropertyPath::PlayerNamePath( playerName ) ); playerId.isString() )
	{
		return playerId.toString();
	}
	return {};
}

bool JsonWrapper::IsNull() const
{
	return document_.isNull();
}

QByteArray JsonWrapper::ToJson( const QJsonDocument::JsonFormat format ) const
{
	return document_.toJson( format );
}

QStringList PropertyPath::ToList() const
{
	return split( '.' );
}

PropertyPath PropertyPath::PlayerPath()
{
	return PropertyPath( "playerNames.value" );
}

PropertyPath PropertyPath::PlayerNamePath( const QString& steamId )
{
	return PropertyPath( PropertyPath( "playerNames.value.%1" ).arg( steamId ) );
}

PropertyPath PropertyPath::PlayerUpgrade( const QString& steamId, const QString& upgradeName )
{
	return PropertyPath( PropertyPath( "dictionaryOfDictionaries.value.%1.%2" ).arg( upgradeName ).arg( steamId ) );
}

PropertyPath PropertyPath::RunStatsPath( const QString& key )
{
	return PropertyPath( PropertyPath( "dictionaryOfDictionaries.value.runStats.%1" ).arg( key ) );
}

PropertyPath PropertyPath::TeamNamePath()
{
	return PropertyPath( "teamName.value" );
}

PropertyPath PropertyPath::DateAndTimePath()
{
	return PropertyPath( "dateAndTime.value" );
}

PropertyPath PropertyPath::TimePlayedPath()
{
	return PropertyPath( "timePlayed.value" );
}

PropertyPath PropertyPath::ItemsPurchasedPath()
{
	return PropertyPath( "dictionaryOfDictionaries.value.itemsPurchased" );
}

PropertyPath PropertyPath::ItemsPurchasedCountPath( const QString& itemName )
{
	return PropertyPath( QString( "dictionaryOfDictionaries.value.itemsPurchased.%1" ).arg( itemName ) );
}

QStringList PropertyPath::ToList( const QString& path )
{
	return path.split( '.' );
}
