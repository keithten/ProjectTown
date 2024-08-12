//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// object collection synchronisation pattern support
// * abstract two differently implemented object collections
// * provide needed basic operations
// * synchronisation process handled with general impl

// common sync algorithm
// * sync two linear, randomly accessible, collections of objects
// * mapping one type to another
// * minimise collection churn and re-allocation of objects, re-use where possible
//
template<typename A,typename B>
struct FSynchroniser
{
	virtual void BeginSync() {}
	virtual int CountA() = 0;
	virtual int CountB() = 0;
	virtual A GetA( int index ) = 0;
	virtual B GetB( int index ) = 0;
	virtual bool MatchAB( A a, B b ) = 0;
	virtual B CreateB( A a ) = 0;
	virtual void SyncAB( A a, B b ) {}
	virtual void DestroyB( B b ) = 0;
	virtual void InsertB( B b, int index ) = 0;
	virtual void RemoveB( int index ) = 0;
	virtual void EndSync() {}
	
	void Sync()
	{
		BeginSync();
		int num_wanted = CountA();
		
		//match and re-use existing
		int index = 0;
		while(index<num_wanted)
		{
			//source
			A a = GetA( index );
			
			//existing?
			if(index<CountB())
			{
				//scan for existing match to use
				for(int j=index ; j<CountB() ; j++)
				{
					B existing_b = GetB( j );
					if(MatchAB( a, existing_b ))
					{
						//found candidate
						if(j==index)
						{
							//already in right place, nothing to do
						}
						else
						{
							//needs moving
							RemoveB( j );
							InsertB( existing_b, index );
						}

						//also do anything else needed to sync them
						SyncAB( a, existing_b );
					}
				}
			}
			else
			{
				//run out, add new instead
				B new_b = CreateB( a );
				InsertB( new_b, index );
			}
			
			//next
			index++;
		}
		
		//remove unwanted
		while(index < CountB())
		{
			//remove remainder
			B old_b = GetB( index );
			RemoveB( index );
			DestroyB( old_b );
		}
		EndSync();
	}
};

//EXAMPLE
#if 0

struct MySync : FSynchroniser<int,float>
{
	//setup
	std::vector<int>& a_list;
	std::vector<float>& b_list;
	MySync(std::vector<int>& in_a_list, std::vector<float>& in_b_list )
	{
		a_list = in_a_list;
		b_list = in_b_list;
	}
	
	//impl
	virtual int CountA() { return a_list.size(); }
	virtual int CountB() { return b_list.size(); }
	virtual A GetA( int index ) { return a_list[index]; }
	virtual B GetB( int index ) { return b_list[index]; }
	virtual bool Match( A a, B b ) { return (int)b == a; }
	virtual B CreateB( A a ) { return (float)a; }
	virtual void DestroyB( B b ) {}
	virtual void InsertB( B b, int index ) { b_list.insert( b_list.begin()+index, b ); }
	virtual void RemoveB( int index ) { b_list.erase( b_list.begin()+index ); }
}

std::vector<int> my_int_list;
std::vector<float> my_float_list;
...
MySync( my_int_list, my_float_list ).Sync();

#endif
