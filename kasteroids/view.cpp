

#include <stdlib.h>
#include <math.h>
#include <kapp.h>
#include <qkeycode.h>
#include "view.h"

#include "view.moc"

#define IMG_BACKGROUND "sprites/bg.ppm"
#define WIDTH 640
#define HEIGHT 480
#define SPRITES_PREFIX kapp->kdedir() + "/share/apps/kasteroids/"

#define ROCK1_IMAGE    "sprites/rock1/rock1-%d.ppm"
#define ROCK1_MASK     "sprites/rock1/rock1-%d.pbm"
#define ROCK1_FRAMES   32

#define ROCK2_IMAGE    "sprites/rock2/rock2-%d.ppm"
#define ROCK2_MASK     "sprites/rock2/rock2-%d.pbm"
#define ROCK2_FRAMES   32

#define ROCK3_IMAGE    "sprites/rock3/rock3-%d.ppm"
#define ROCK3_MASK     "sprites/rock3/rock3-%d.pbm"
#define ROCK3_FRAMES   32

#define SHIP_IMAGE     "sprites/ship/ship%d.ppm"
#define SHIP_MASK      "sprites/ship/ship%d.pbm"
#define SHIP_FRAMES    32

#define MISSILE_IMAGE  "sprites/missile/missile%d.ppm"
#define MISSILE_MASK   "sprites/missile/missile%d.pbm"
#define MISSILE_FRAMES 1

#define BITS_IMAGE     "sprites/bits/bits%d.ppm"
#define BITS_MASK      "sprites/bits/bits%d.pbm"
#define BITS_FRAMES    8

#define REFRESH_DELAY  25
#define SHIP_SPEED     0.2
#define MISSILE_SPEED  10.0


KAsteroidsView::KAsteroidsView( QWidget *parent, const char *name )
    : QWidget( parent, name ), field(SPRITES_PREFIX + IMG_BACKGROUND,WIDTH,HEIGHT),
      view(&field,this)
{
    rocks.setAutoDelete( true );
    missiles.setAutoDelete( true );
    bits.setAutoDelete( true );

    readSprites();
}

KAsteroidsView::~KAsteroidsView()
{
}

void KAsteroidsView::reset()
{
    rocks.clear();
    missiles.clear();
    bits.clear();

    rotateL    = false;
    rotateR    = false;
    thrustShip = false;
    shootShip  = false;

    shipDx = 0.0;
    shipDy = 0.0;

    rockSpeed = 1.0;
}

void KAsteroidsView::newGame()
{
    reset();
    startTimer( REFRESH_DELAY );
}

void KAsteroidsView::endGame()
{
    killTimers();
}

void KAsteroidsView::newShip()
{
    ship->moveTo( width()/2, height()/2, 0 );
    ship->setVelocity( 0.0, 0.0 );
    shipDx = 0;
    shipDy = 0;
    rotateL = false;
    rotateR = false;
    thrustShip = false;
    shootShip = false;

    ship->show();
}

void KAsteroidsView::readSprites()
{
    largeRockImg = new QwSpritePixmapSequence(
	SPRITES_PREFIX + ROCK1_IMAGE, 
	SPRITES_PREFIX + ROCK1_MASK, 
	ROCK1_FRAMES );

    mediumRockImg = new QwSpritePixmapSequence(
	SPRITES_PREFIX + ROCK2_IMAGE, 
	SPRITES_PREFIX + ROCK2_MASK, 
	ROCK2_FRAMES );

    smallRockImg = new QwSpritePixmapSequence(
	SPRITES_PREFIX + ROCK3_IMAGE, 
	SPRITES_PREFIX + ROCK3_MASK, 
	ROCK3_FRAMES );

    bitImg = new QwSpritePixmapSequence(
	SPRITES_PREFIX + BITS_IMAGE, 
	SPRITES_PREFIX + BITS_MASK, 
	BITS_FRAMES );

    QwSpritePixmapSequence *images = new QwSpritePixmapSequence(
	SPRITES_PREFIX + SHIP_IMAGE, 
	SPRITES_PREFIX + SHIP_MASK, 
	SHIP_FRAMES );

    ship = new QwRealMobileSprite( *images );
    ship->setBoundsAction( QwRealMobileSprite::Wrap );
    ship->hide();

    missileImg = new QwSpritePixmapSequence( 
	SPRITES_PREFIX + MISSILE_IMAGE,
	SPRITES_PREFIX + MISSILE_MASK, 
	MISSILE_FRAMES );
}

void KAsteroidsView::addRocks( int num )
{
    int i;

    for ( i = 0; i < num; i++ )
    {
	KLargeRock *rock = new KLargeRock( *largeRockImg );
	rock->setBoundsAction( QwRealMobileSprite::Wrap );
	rock->setVelocity( 2.0 - (double)(random()%256)/64.0,
			    2.0 - (double)(random()%256)/64.0 );
	switch ( random()%4 )
	{
	    case 0:
		rock->moveTo( 0, 0, 0 );
		break;

	    case 1:
		rock->moveTo( 639, 0, 0 );
		break;

	    case 2:
		rock->moveTo( 639, 479, 0 );
		break;

	    default:
		rock->moveTo( 0, 479, 0 );
		break;
	}

	rocks.append( rock );
    }
}

void KAsteroidsView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    view.resize(width(),height());
    field.resize(width(),height());

    QwRealMobileSprite *rock;

    for ( rock = rocks.first(); rock; rock = rocks.next() )
    {
	rock->adoptPlayfieldBounds();
    }

    QwRealMobileSprite *missile;

    for ( missile = missiles.first(); missile; missile = missiles.next() )
    {
	missile->adoptPlayfieldBounds();
    }

    ship->adoptPlayfieldBounds();
}

void KAsteroidsView::timerEvent( QTimerEvent * )
{
    QwRealMobileSprite *rock;

    // move rocks forward
    for ( rock = rocks.first(); rock; rock = rocks.next() )
    {
	rock->forward( rockSpeed );
	rock->frame( ( rock->frame()+1 ) % ROCK1_FRAMES );
    }

    // move missiles and check for collision with rocks.
    processMissiles();

    // these are generated when a ship explodes
    for ( KBit *bit = bits.first(); bit; bit = bits.next() )
    {
	if ( bit->expired() )
	{
	    bit->hide();
	    bits.remove();
	}
	else
	{
	    bit->forward( 2.0 );
	    bit->growOlder();
	    bit->frame( ( bit->frame()+1 ) % BITS_FRAMES );
	}
    }

    // move / rotate ship.
    // check for collision with a rock.
    processShip();

    field.update();

    killTimers();
    startTimer(REFRESH_DELAY);
}

void KAsteroidsView::processMissiles()
{
    QwSpriteFieldGraphic *hit;
    KMissile *missile;

    // move missiles forward
    for ( missile = missiles.first(); missile; missile = missiles.next() )
	missile->forward( MISSILE_SPEED );

    // if a missile has hit a rock, remove missile and break rock into smaller
    // rocks or remove completely.
    QListIterator<KMissile> it(missiles);

    for ( ; it.current(); ++it )
    {
	missile = it.current();
	missile->growOlder();

	if ( missile->expired() )
	{
	    missile->hide();
	    missiles.removeRef( missile );
	    continue;
	}

	Pix p;
	p = missile->neighbourhood( missile->frame() );

	hit = missile->at( p );
	while ( hit && ( hit == ship || !missile->exact( p ) ) )
	{
	    missile->next( p );
	    hit = missile->at( p );
	}

	missile->end( p );

	if ( hit && hit != ship )
	{
	    hit->hide();
	    if ( hit->rtti() == LARGE_ROCK || hit->rtti() == MEDIUM_ROCK )
	    {
		// break into smaller rocks
		double dx, dy;
		double addx[4] = { 1.0, 1.0, -1.0, -1.0 };
		double addy[4] = { -1.0, 1.0, -1.0, 1.0 };

		QwRealMobileSprite *rHit = (QwRealMobileSprite *) hit;
		rHit->getVelocity( dx, dy );

		QwRealMobileSprite *nrock;

		for ( int i = 0; i < 4; i++ )
		{
		    double r = (double)(random()%10)/15;
		    if ( hit->rtti() == LARGE_ROCK )
		    {
			nrock = new KMediumRock( *mediumRockImg );
			emit rockHit( 0 );
		    }
		    else
		    {
			nrock = new KSmallRock( *smallRockImg );
			emit rockHit( 1 );
		    }

		    nrock->setBoundsAction( QwRealMobileSprite::Wrap );
		    nrock->moveTo( rHit->x(), rHit->y(), 0 );
		    nrock->setVelocity( dx+addx[i]+r, dy+addy[i]+r );
		    rocks.append( nrock );
		}
	    }
	    else if ( hit->rtti() == SMALL_ROCK )
		emit rockHit( 2 );
	    rocks.removeRef( (const QwRealMobileSprite *)hit );
	    if ( rocks.count() == 0 )
		emit rocksRemoved();
	    missile->hide();
	    missiles.removeRef( missile );
	}
    }
}

void KAsteroidsView::processShip()
{
    QwSpriteFieldGraphic *hit;
    Pix sp;
    int i;

    if ( ship->visible() )
    {
	sp = ship->neighbourhood( ship->frame() );
	hit = ship->at( sp );

	while ( hit )
	{
	    if ( hit->rtti() >= LARGE_ROCK && hit->rtti() <= SMALL_ROCK )
	    {
		if ( ship->exact( sp ) )
		{
		    KBit *bit;
		    for ( i = 0; i < 12; i++ )
		    {
			bit = new KBit( *bitImg );
			bit->setBoundsAction( QwRealMobileSprite::Wrap );
			bit->moveTo( ship->exact_x(), ship->exact_y(), 
				    random()%BITS_FRAMES );
			bit->setVelocity( 0.5-(double)(random()%20)/20.0,
					0.5-(double)(random()%20)/20.0 );
			bit->setDeath( 60 + ( random() % 60 ) );
			bits.append( bit );
		    }
		    ship->hide();
		    emit shipKilled();
		    break;
		}
	    }
	    ship->next( sp );
	    hit = ship->at( sp );
	}

	int frame = ship->frame();

	if ( rotateL )
	{
	    if ( --frame < 0 )
		frame = SHIP_FRAMES-1;
	}
	if ( rotateR )
	{
	    if ( ++frame == SHIP_FRAMES )
		frame = 0;
	}

	double angle = frame * 2 * M_PI / 32.0;
	double cosangle = cos( angle );
	double sinangle = sin( angle );

	if ( thrustShip )
	{
	    shipDx += cosangle;
	    shipDy += sinangle;
	    ship->setVelocity( shipDx, shipDy );
	}

	ship->frame( frame );
	ship->forward( SHIP_SPEED );

	if ( shootShip )
	{
	    if ( missiles.count() < 5 )
	    {
		KMissile *missile = new KMissile( *missileImg );
		missile->setBoundsAction( QwRealMobileSprite::Wrap );
		missile->moveTo( ship->exact_x()+cosangle*24,
				ship->exact_y()+sinangle*24, 0 );
		missile->setVelocity( cosangle, sinangle );
		missiles.append( missile );
	    }
	    shootShip = false;
	}
    }
}
