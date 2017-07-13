#include <iostream>
#include <map>
#include <iomanip>
#include <vector>
#include <string>
#include <stdlib.h>
#include <sstream>

class TextTable {

    public:
    enum class Alignment { LEFT, RIGHT }; 
    typedef std::vector< std::string > Row;
    TextTable( char horizontal = '-', char vertical = '|', char corner = '+' ) :
        _horizontal( horizontal ),
        _vertical( vertical ),
        _corner( corner )
    {}

    void setAlignment( unsigned i, Alignment alignment )
    {
        _alignment[ i ] = alignment;
    }

    Alignment alignment( unsigned i ) const
    { return _alignment[ i ]; }

    char vertical() const
    { return _vertical; }

    char horizontal() const
    { return _horizontal; }

    void setTitle( std::string title) 
    {
        _title = title;
    }

    std::string getTitle( void ) const
    {
        return _title;
    }

    void add( std::string const & content )
    {
        _current.push_back( content );
    }

    void add( double const & content )
    {
        if(content < 0.0)
            _current.push_back( std::string("N/A") );
        else if(content < 0.01)
            _current.push_back( std::string("< 0.01") );
        else {
            std::stringstream tmp;
            tmp << std::fixed << std::setprecision(2) << content;
            _current.push_back( tmp.str() );
        }
    }

    void add( unsigned int const & content )
    {
        std::stringstream tmp;
        tmp << content;
        _current.push_back( tmp.str() );
    }

    void endOfRow()
    {
        _rows.push_back( _current );
        _current.assign( 0, "" );
    }

    template <typename Iterator>
    void addRow( Iterator begin, Iterator end )
    {
        for( auto i = begin; i != end; ++i ) {
           add( * i ); 
        }
        endOfRow();
    }

    template <typename Container>
    void addRow( Container const & container )
    {
        addRow( container.begin(), container.end() );
    }

    std::vector< Row > const & rows() const
    {
        return _rows;
    }

    void setup() const
    {
        determineWidths();
        setupAlignment();
    }

    std::string ruler() const
    {
        std::string result;
        result += _corner;
        for( auto width = _width.begin(); width != _width.end(); ++ width ) {
            result += repeat( * width, _horizontal );
            result += _corner;
        }
        return result;
    }

    std::string flat_ruler() const
    {
        std::string result;
        result += _corner;
        for( auto width = _width.begin(); width != _width.end(); ++ width ) {
            result += repeat( * width, _horizontal );
            if ( width == _width.end()-1 )
                result += _corner;
            else
               result += _horizontal; 
        }
        return result;
    }

    int width( unsigned i ) const
    { return _width[ i ]; }

    int tableWidth()  const
    { 
        int tableWidth=0;
        for( auto width = _width.begin(); width != _width.end(); ++ width ) {
            tableWidth += *width +1;
        }
        return tableWidth-1;
    }

    private:
    char _horizontal;
    char _vertical;
    char _corner;
    Row _current;
    std::vector< Row > _rows;
    std::vector< unsigned > mutable _width;
    std::map< unsigned, Alignment > mutable _alignment;
    int _tableWidth;
    std::string _title;

    static std::string repeat( unsigned times, char c )
    {
        std::string result;
        for( ; times > 0; -- times )
            result += c;

        return result;
    }

    unsigned columns() const
    {
        return _rows[ 0 ].size();
    }

    void determineWidths() const
    {
        _width.assign( columns(), 0 );        
        for ( auto rowIterator = _rows.begin(); rowIterator != _rows.end(); ++ rowIterator ) {
            Row const & row = * rowIterator;
            for ( unsigned i = 0; i < row.size(); ++i ) {
                _width[ i ] = _width[ i ] > row[ i ].size() ? _width[ i ] : row[ i ].size();
            }
        }
    }

    void setupAlignment() const
    {
        for ( unsigned i = 0; i < columns(); ++i ) {
            if ( _alignment.find( i ) == _alignment.end() ) {
                _alignment[ i ] = Alignment::LEFT;
            }
        }
    }
};



std::ostream & operator<<( std::ostream & stream, TextTable const & table )
{
    table.setup();

    // Table Title
    stream << table.flat_ruler() << "\n";
    stream << table.vertical();
    stream << "\x1B[0;1m" << std::setfill(' ') << std::setw( table.tableWidth() ) << std::left << table.getTitle() << "\x1B[0m";
    stream << table.vertical();
    stream << "\n" << table.flat_ruler() << "\n";

    // Table Content
    for ( auto rowIterator = table.rows().begin(); rowIterator != table.rows().end(); ++ rowIterator ) {
        TextTable::Row const & row = * rowIterator;
        stream << table.vertical();
        for ( unsigned i = 0; i < row.size(); ++i ) {
            auto alignment = table.alignment( i ) == TextTable::Alignment::LEFT ? std::left : std::right;
            if (i==0 && rowIterator != table.rows().begin())
                stream << "\x1B[0;34m" << std::setw( table.width( i ) ) << alignment << row[ i ] << "\x1B[0m";
            else if (rowIterator == table.rows().begin()) 
                stream  << std::setw( table.width( i ) ) << alignment << row[ i ] ;  

            // i=1: Compression result column
            else if (i==1 && row[i]=="FAIL")
                stream << "\x1B[0;31m" << std::setw( table.width( i ) ) << alignment << row[ i ] << "\x1B[0m"; 
            else if (i==1 && row[i]=="SUCCESS")
                stream << "\x1B[0;32m" << std::setw( table.width( i ) ) << alignment << row[ i ] << "\x1B[0m";   

            else
                stream << std::setw( table.width( i ) ) << alignment << row[ i ];
            stream << table.vertical();
        }
        stream << "\n";
        stream << table.ruler() << "\n";
        usleep(1);
    }

    return stream;
}
